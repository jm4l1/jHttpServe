#include "HttpServer.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <utility>

namespace fs = std::filesystem;

void HttpServer::Get(std::string target, std::function<HttpResponse(HttpRequest&&)> get_handler)
{
	_route_map.RegisterRoute("GET" + target, get_handler);
}

void HttpServer::Post(std::string target, std::function<HttpResponse(HttpRequest&&)> post_handler)
{
	_route_map.RegisterRoute("POST" + target, post_handler);
}

void HttpServer::Init(std::string config_file_name)
{
	ParseConfigFile(config_file_name);
	if (!_config.HasKey("upload_dir"))
	{
		_config["upload_dir"] = "../uploads";
	}
	int port = static_cast<int>(_config["port"]);
	_server_socket.SetPort(port, PROTO::TCP);
	_server_socket.CreateSocket();

	_socket_thread = std::jthread(std::bind_front(&HttpServer::PerformSocketTask, this));
	_app_logic_thread = std::jthread(std::bind_front(&HttpServer::HandleApplicationLayer, this));
	while (_is_server_running)
	{
		std::mutex application_state_mutex;
		{
			std::unique_lock lock(application_state_mutex);
			_application_state_cond_var.wait_for(lock,
												 std::chrono::seconds(1),
												 [this]() -> bool
												 {
													 return _is_server_running == false;
												 });
		}
		if (!_is_server_running)
		{
			std::cout << "[HttpServer] - Server Closing down\n";
			return;
		}
		if (!_connections.empty())
		{
			_connections.erase(std::remove_if(_connections.begin(),
											  _connections.end(),
											  [](auto& connection)
											  {
												  auto can_remove =
													  (connection->CanClose() ||
													   std::chrono::steady_clock::now() - connection->LastUsedTime() > CONNECTION_TIMEOUT);
												  return can_remove;
											  }),
							   _connections.end());
		}
	}
};

void HttpServer::PerformSocketTask(std::stop_token stop_token)
{
	std::cout << "[HttpServer] - Starting socket receiver\n";
	try
	{
		if (!_server_socket.Bind())
		{
			throw std::runtime_error("Unable to bind to port\n");
		}
		_server_socket.Listen();
		while (!stop_token.stop_requested() && _is_server_running)
		{
			auto accept_result = _server_socket.Accept();
			if (accept_result)
			{
				std::unique_ptr<jSocket> connection_socket = std::move(accept_result);
				auto socket_read_result = connection_socket->Read();
				auto read_error = std::get_if<ReadError>(&socket_read_result);
				if (read_error)
				{
					switch (*read_error)
					{
					case ReadError::ConnectionClosed:
						std::cout << "[HttpServer] - Socket closed by peer\n";
						break;

					case ReadError::TimeOut:
						break;

					default:
						std::cout << "[HttpServer] - Error reading\n";
						break;
					}
					continue;
				}
				auto buffer = *(std::get_if<std::vector<unsigned char>>(&socket_read_result));
				if (!buffer.empty())
				{
					ParseData(std::move(buffer), std::move(connection_socket));
				}
			}
		}
	}
	catch (const std::exception& e)
	{
		std::cout << "[HTTPServer] - error " << e.what();
	}
	_is_server_running = false;
	std::cout << "[HttpServer] - Ending socket receiver\n";
}

void HttpServer::ParseData(std::vector<unsigned char>&& message_buffer, std::unique_ptr<jSocket> socket)
{
	HttpRequest request = HttpRequest(std::move(message_buffer));
	if (!request.isValid)
	{
		std::cout << "[HttpServer] - Unable to parse data as http request\n";
		HttpResponse response = HttpResponse();
		response.SetHeader("server", (std::string)_config["server_name"]);
		response.SetHeader("date", GetDate());
		response.SetHeader("connection", request.GetHeader("Connection").value_or("close"));
		response.SetStatusCode(400);
		Log(request, response);
		socket->Write(response.ToBuffer());
		return;
	}
	auto req_res = std::make_pair(std::move(request), std::move(socket));
	_request_queue.Send(std::move(req_res));
	return;
}

void HttpServer::Log(const HttpRequest& request, const HttpResponse& response)
{
	auto status_code = response.GetStatusCode();
	auto formatted_status_start = status_code < 200 ? "\033[1;34m" :
								  status_code < 300 ? "\033[1;32m" :
								  status_code < 400 ? "\033[1;33m" :
													  "\033[1;31m";
	auto formatted_status_end = "\033[0m";

	std::lock_guard<std::mutex> lock(_logger_mutex);
	std::cout << "[" << response.GetHeader("date").value_or("") << "] " << request.GetStartLine() << " " << formatted_status_start
			  << status_code << formatted_status_end << " -\n";
};

void HttpServer::HandleApplicationLayer(std::stop_token stop_token)
{
	std::cout << "[HttpServer] - Application Layer Started\n";
	std::stringstream body_stream;
	while (!stop_token.stop_requested() && _is_server_running)
	{
		auto receiveResult = _request_queue.TryReceive(std::chrono::milliseconds(500));
		if (!receiveResult.has_value())
		{
			continue;
		}
		std::cout << "[HttpServer] - received request\n";
		auto [request, socket] = std::move(receiveResult.value());
		std::unique_ptr<HttpConnection> connection = std::make_unique<HttpConnection>(std::move(socket));
		connection->SetDataHandler(
			[this](const std::vector<unsigned char>& data_buffer) -> std::optional<HttpResponse>
			{
				{
					HttpRequest http_request(data_buffer);
					if (http_request.isValid)
					{
						return HandleHttpRequest(std::move(http_request));
					}
				}
				std::cout << "[HttpServer] - Unknown data received\n";
				return std::nullopt;
			});
		connection->HandleData(request.ToBuffer());
		_connections.emplace_back(std::move(connection));
	}
	std::cout << "[HttpServer] - Application Layer Ending\n";
	_is_server_running = false;
};

HttpResponse HttpServer::HandleHttpRequest(HttpRequest&& request)
{
	HttpResponse response = HttpResponse();
	response.SetHeader("server", (std::string)_config["server_name"]);
	response.SetHeader("date", GetDate());
	response.SetHeader("connection", request.GetHeader("Connection").value_or("close"));
	std::stringstream body_stream;
	auto method = request.GetMethod();
	auto target = request.GetTarget();

	// check method allowed
	if (!ValidateMethod(method))
	{
		std::cout << "[HttpServer] - Http Validation failed for method " << method << "\n";
		response.SetStatusCode(405);
		response.SetHeader("connection", "close");
		std::stringstream allowed_stream;
		std::ostream_iterator<std::string> outputString(allowed_stream, ",");
		std::copy(_allowed_methods.begin(), _allowed_methods.end(), outputString);
		response.SetHeader("allow", allowed_stream.str());
		body_stream << "<body><div><H1>405 Method Not Allowed</H1>" << allowed_stream.str() << "</div></body>";
		response.SetBody(body_stream.str());
		Log(request, response);
		return response;
	}
	// check route map for requested resource
	auto request_handler = _route_map.GetRouteHandler(method + target).value_or(nullptr);
	if (request_handler)
	{
		return request_handler(std::move(request));
	}
	target = target == "/" ? "/index.html" : target;
	if (target == "/upload")
	{
		if (method == "POST")
		{
			return HandleUpload(std::move(request));
		}
		if (method == "GET")
		{
			return HandleGetUploads(std::move(request));
		}
	}
	if (auto pos = target.find("/upload/") != std::string::npos)
	{
		if (method == "GET")
		{
			auto filename = std::string(target.begin() + 8, target.end());
			auto file_location = (std::string)_config["upload_dir"] + "/" + filename;
			if (!fs::exists(fs::path(file_location)))
			{
				response.SetStatusCode(404);
				response.SetHeader("content-type", "text/html;charset=utf-8");
				body_stream << "<body><div><H1>404 Not Found</H1>" << filename << " not found.</div></body>";
				std::vector<unsigned char> body_vec((std::istreambuf_iterator<char>(body_stream)), std::istreambuf_iterator<char>());
				response.SetBody(body_vec);
				Log(request, response);
				return response;
			}
			std::ifstream target_file(file_location, std::ios::binary);
			if (!target_file.is_open())
			{
				response.SetStatusCode(500);
				response.SetHeader("content-type", "text/html;charset=utf-8");
				body_stream << "<body><H1>500 Internal Server Error</H1><div>.</div></body>";
				std::vector<unsigned char> body_vec((std::istreambuf_iterator<char>(body_stream)), std::istreambuf_iterator<char>());
				response.SetBody(body_vec);
				Log(request, response);
				return response;
			}

			target_file.unsetf(std::ios::skipws);
			std::vector<unsigned char> target_file_contents((std::istream_iterator<char>(target_file)), std::istream_iterator<char>());

			response.SetStatusCode(200);
			response.SetHeader("content-type", "application/octet-stream");
			response.SetHeader("Content-Disposition", R"(inline; filename=")" + filename + R"(")");
			response.SetBody(target_file_contents);
			Log(request, response);
			return response;
		}

		response.SetStatusCode(405);
		response.SetHeader("allow", "GET");
		body_stream << "<body><div><H1>405 Method Not Allowed</H1><p>The request method " << method << " is not appropriate for the target "
					<< target << ".</p></div></body>";
		response.SetBody(body_stream.str());
		Log(request, response);
		return response;
	}
	// fall back to web dir
	auto target_location = (std::string)_config["web_dir"] + "/" + target;
	if (!fs::exists(fs::path(target_location)))
	{
		response.SetStatusCode(404);
		response.SetHeader("content-type", "text/html;charset=utf-8");
		body_stream << "<body><div><H1>404 Not Found</H1>" << target << " not found.</div></body>";
		std::vector<unsigned char> body_vec((std::istreambuf_iterator<char>(body_stream)), std::istreambuf_iterator<char>());
		response.SetBody(body_vec);
		Log(request, response);
		return response;
	}
	std::ifstream target_file(target_location, std::ios::binary);
	if (!target_file.is_open())
	{
		response.SetStatusCode(500);
		response.SetHeader("content-type", "text/html;charset=utf-8");
		body_stream << "<body><H1>500 Internal Server Error</H1><div>.</div></body>";
		std::vector<unsigned char> body_vec((std::istreambuf_iterator<char>(body_stream)), std::istreambuf_iterator<char>());
		response.SetBody(body_vec);
		Log(request, response);
		return response;
	}
	std::vector<unsigned char> target_file_contents((std::istreambuf_iterator<char>(target_file)), std::istreambuf_iterator<char>());
	response.SetStatusCode(200);
	response.SetHeader("content-type", "text/html;charset=utf-8");
	response.SetBody(target_file_contents);
	Log(request, response);
	return response;
};

HttpResponse HttpServer::HandleUpload(HttpRequest&& request)
{
	HttpResponse response;
	response.SetHeader("connection", request.GetHeader("Connection").value_or("close"));
	response.SetHeader("server", (std::string)_config["server_name"]);
	response.SetHeader("date", GetDate());
	auto accepted_content_type = std::vector<std::string>{ "multipart/form-data", "text/plain" };
	auto content_type = request.GetHeader("Content-Type").value_or("");
	bool acceptable_type = false;
	if (content_type == "")
	{
		content_type = "text/plain";
		acceptable_type = true;
	}
	else
	{
		for (auto type : accepted_content_type)
		{
			if (content_type.find(type) != std::string::npos)
			{
				acceptable_type = true;
			}
		}
	}
	if (!acceptable_type)
	{
		response.SetStatusCode(415);
		response.SetHeader("accept", "multipart/form-data , text/plain");
		Log(request, response);
		return response;
	}
	if (!fs::exists(fs::path(_config["upload_dir"])))
	{
		fs::create_directory((std::string)_config["upload_dir"]);
	}
	if (content_type.find("text/plain") != std::string::npos)
	{
		auto file_location = (std::string)_config["upload_dir"] + "/" + "file" + GetshortDate();
		auto upload_file = std::ofstream(file_location, std::ios::binary);
		if (!upload_file)
		{
			response.SetStatusCode(500);
			Log(request, response);
			return response;
		}
		try
		{
			auto body_buffer = request.GetBody();
			upload_file.write(((char*)body_buffer.data()), body_buffer.size());
		}
		catch (...)
		{
			response.SetStatusCode(500);
			Log(request, response);
			return response;
		}
	}
	else if (content_type.find("multipart/form-data") != std::string::npos)
	{
		auto boundary_start = content_type.find("boundary");
		if (boundary_start == std::string::npos)
		{
			response.SetStatusCode(400);
			Log(request, response);
			return response;
		}
		auto boundary_param = std::string(content_type.begin() + boundary_start, content_type.end());
		boundary_start = boundary_param.find("=");
		if (boundary_start == std::string::npos)
		{
			response.SetStatusCode(400);
			Log(request, response);
			return response;
		}
		auto boundary_value = std::string(boundary_param.begin() + boundary_start + 1, boundary_param.end());
		auto body_buffer = request.GetBody();
		body_buffer.erase(body_buffer.begin(), body_buffer.begin() + boundary_value.size() + 4);
		body_buffer.erase(std::search(body_buffer.begin(), body_buffer.end(), boundary_value.begin(), boundary_value.end()) - 2,
						  body_buffer.end());
		// get headers
		auto headers_end = std::string((char*)body_buffer.data()).find("\r\n\r\n");
		if (headers_end == std::string::npos)
		{
			response.SetStatusCode(500);
			Log(request, response);
			return response;
		}
		auto headers_string = std::string((char*)body_buffer.data(), ((char*)body_buffer.data()) + headers_end);
		body_buffer.erase(body_buffer.begin(), body_buffer.begin() + headers_end + 4);
		std::string file_name;
		ssize_t f_start;
		if ((f_start = headers_string.find("filename")) != std::string::npos)
		{
			auto name_start = headers_string.find_first_of('"', f_start);
			auto name_end = headers_string.find_first_of('"', name_start + 1);
			file_name = std::string(headers_string.begin() + name_start + 1, headers_string.begin() + name_end);
		}
		else if ((f_start = headers_string.find("name=")) != std::string::npos)
		{
			auto name_start = headers_string.find_first_of('"', f_start);
			auto name_end = headers_string.find_first_of('"', name_start);
			file_name = std::string(headers_string.begin() + name_start, headers_string.begin() + name_end);
		}
		else
		{
			file_name = "file" + GetshortDate();
		}
		auto file_location = (std::string)_config["upload_dir"] + "/" + file_name;
		auto upload_file = std::ofstream(file_location, std::ios::binary);
		if (!upload_file)
		{
			response.SetStatusCode(500);
			Log(request, response);
			return response;
		}
		try
		{
			upload_file.write(((char*)body_buffer.data()), body_buffer.size());
		}
		catch (...)
		{
			response.SetStatusCode(500);
			Log(request, response);
			return response;
		}
	}
	response.SetHeader("content-type", "application/json");
	auto api_object = jjson::Object();
	api_object["status"] = "Success!!!";
	api_object["message"] = "Upload Complete";
	response.SetBody(api_object);
	response.SetStatusCode(200);
	Log(request, response);
	return response;
}

HttpResponse HttpServer::HandleGetUploads(HttpRequest&& request)
{
	HttpResponse response;
	std::stringstream body_stream;
	auto upload_dir = (std::string)_config["upload_dir"];
	if (!fs::exists(fs::path(upload_dir)))
	{
		response.SetHeader("content-type", "text/html;charset=utf-8");
		body_stream << "<body><H1>500 Internal Server Error</H1><div>.</div></body>";
		std::vector<unsigned char> body_vec((std::istreambuf_iterator<char>(body_stream)), std::istreambuf_iterator<char>());
		response.SetBody(body_vec);
		Log(request, response);
		return response;
	}
	body_stream << "<!DOCTYPE html><htm><body><H1>List of Uploads</H1><div><ul>";
	for (auto& p : fs::directory_iterator(upload_dir))
	{
		std::string file_name = p.path().filename().string();
		// ignore hidden file
		if (file_name[0] == '.')
		{
			continue;
		}
		file_name.erase(std::remove(file_name.begin(), file_name.end(), '\"'), file_name.end());
		body_stream << R"(<li><a href="/upload/)" << file_name << R"(">)" << p.path().filename() << "</li>";
	}
	body_stream << "</ul></div></body></html>";
	response.SetHeader("content-type", "text/html;charset=utf-8");
	response.SetHeader("server", (std::string)_config["server_name"]);
	response.SetHeader("date", GetDate());
	response.SetHeader("connection", request.GetHeader("Connection").value_or("close"));
	std::vector<unsigned char> body_vec((std::istreambuf_iterator<char>(body_stream)), std::istreambuf_iterator<char>());
	response.SetBody(body_vec);
	response.SetStatusCode(200);
	Log(request, response);
	return response;
}

void HttpServer::ParseConfigFile(std::string file_name)
{
	if (!fs::exists(fs::path(file_name)))
	{
		std::cout << "Config file could not be found!!\n";
		exit(EXIT_FAILURE);
	}

	std::ifstream config_file(file_name);
	if (!config_file.is_open())
	{
		std::cout << "Error Opening Config file!\n";
		exit(EXIT_FAILURE);
	}

	std::string config_string;
	std::string line;
	while (std::getline(config_file, line))
	{
		config_string.append(line);
	}

	_config = jjson::value::parse_from_string(config_string);
	if (!_config.is_valid())
	{
		std::cout << "Error Parsing Config file. Please verify valid JSON\n";
		exit(EXIT_FAILURE);
	}
	if (!_config.HasKey("port"))
	{
		std::cout << "port is required in config file\n";
		exit(EXIT_FAILURE);
	}
	if (!_config.HasKey("web_dir"))
	{
		std::cout << "web_dir location is required in config file\n";
		exit(EXIT_FAILURE);
	}
	std::string web_dir = (std::string)_config["web_dir"];
	if (!fs::exists(fs::path(web_dir)))
	{
		std::cout << "web dir could not be found!\n";
		exit(EXIT_FAILURE);
	}
	if (_config.HasKey("allowed_methods"))
	{
		_allowed_methods = (std::vector<std::string>)_config["allowed_methods"];
	}
	else
	{
		_allowed_methods = Methods;
	}
	std::cout << "Config file loaded!\n";
}