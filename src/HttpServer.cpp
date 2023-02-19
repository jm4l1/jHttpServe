#include "HttpServer.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>


namespace fs = std::filesystem;

void HttpServer::Get(std::string target, std::function<void(HttpRequest&&, HttpResponse&&)> get_handler)
{
	_route_map.RegisterRoute("GET" + target, get_handler);
}
void HttpServer::Post(std::string target, std::function<void(HttpRequest&&, HttpResponse&&)> post_handler)
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
	int port = (int)_config["port"];
	_socket_server.SetPort(port);
	_socket_server.SetTimeout((int)_config["timeout"]);
	_socket_server.CreateSocket();
	auto socket_thread = std::async(std::launch::async,
									[this]()
									{
										_socket_server.Listen(handle_parse_layer);
									});
	auto app_logic_thread = std::async(std::launch::async, &HttpServer::HandleApplicationLayer, this);
};
void HttpServer::Log(const HttpRequest& request, const HttpResponse& response)
{
	std::lock_guard<std::mutex> lock(_logger_mutex);
	auto status_code = response.GetStatusCode();
	auto formatted_status_start = status_code < 200 ? "\033[1;34m" :
								  status_code < 300 ? "\033[1;32m" :
								  status_code < 400 ? "\033[1;33m" :
													  "\033[1;31m";
	auto formatted_status_end = "\033[0m";
	std::cout << "[" << response.GetHeader("date").value_or("") << "] " << request.GetStartLine() << " " << formatted_status_start
			  << status_code << formatted_status_end << " -\n";
};
void HttpServer::HandleApplicationLayerSync(HttpRequest&& request, HttpResponse&& response)
{
	std::stringstream body_stream;
	auto method = request.GetMethod();
	auto target = request.GetTarget();
	response.SetHeader("server", (std::string)_config["server_name"]);
	response.SetHeader("date", GetDate());
	response.SetHeader("connection", "close");

	// check method allowed
	ValidateMethod(method, request, response);
	// ch&eck route map for requested resource
	auto request_handler = _route_map.GetRouteHandler(method + target).value_or(nullptr);
	if (request_handler)
	{
		request_handler(std::move(request), std::move(response));
		return;
	}
	target = target == "/" ? "/index.html" : target;
	if (target == "/upload")
	{
		if (method == "POST")
		{
			HandleUpload(std::move(request), std::move(response));
			return;
		}
		if (method == "GET")
		{
			HandleGetUploads(std::move(request), std::move(response));
			return;
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
				response.Send();
				return;
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
				response.Send();
				return;
			}

			target_file.unsetf(std::ios::skipws);
			std::vector<unsigned char> target_file_contents((std::istream_iterator<char>(target_file)), std::istream_iterator<char>());

			response.SetStatusCode(200);
			response.SetHeader("content-type", "application/octet-stream");
			response.SetHeader("Content-Disposition", R"(inline; filename=")" + filename + R"(")");
			response.SetBody(target_file_contents);
			Log(request, response);
			response.Send();
			return;
		}
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
		response.Send();
		return;
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
		response.Send();
		return;
	}
	std::vector<unsigned char> target_file_contents((std::istreambuf_iterator<char>(target_file)), std::istreambuf_iterator<char>());
	response.SetStatusCode(200);
	response.SetHeader("content-type", "text/html;charset=utf-8");
	response.SetBody(target_file_contents);
	Log(request, response);
	response.Send();
	return;
};
void HttpServer::HandleApplicationLayerHttp2Sync(HttpRequest&& request, HttpResponse&& response)
{
	response.SetStatusCode(101);
	response.SetHeader("Connection", "Upgrade");
	response.SetHeader("Upgrade", "h2c");

	auto settings_max_frame_size = Http2SettingsParam(SETTINGS_MAX_FRAME_SIZE, 16384);
	auto settings_enable_push = Http2SettingsParam(SETTINGS_ENABLE_PUSH, 3);
	auto settings_intial_window_size = Http2SettingsParam(SETTINGS_INITIAL_WINDOW_SIZE, 1234);
	auto settings_frame = Http2SettingsFrame();
	settings_frame.AddParam(settings_max_frame_size);
	settings_frame.AddParam(settings_enable_push);
	settings_frame.AddParam(settings_intial_window_size);

	auto frame = Http2Frame();
	frame.type = HTTP2_SETTINGS_FRAME;
	frame.length = settings_frame.Size();
	frame.flags = 0x0;
	frame.stream_id = 0x0;
	frame.payload = settings_frame.Serialize();

	// std::cout << CONNECTION_PREFACE[0] << "\n";
	std::vector<unsigned char> connection_preface(CONNECTION_PREFACE, (CONNECTION_PREFACE) + strlen(CONNECTION_PREFACE));
	// response.SetBody(conneciton_preface);
	// response.SetBody(frame.Serialize());
	response.AppendToBody(frame.Serialize());
	response.AppendToBody(connection_preface);

	Log(request, response);
	response.Send();
	return;
};
void HttpServer::HandleApplicationLayer()
{
	std::cout << "Application Layer Started\n";
	std::stringstream body_stream;
	while (1)
	{
		auto req_res = _request_queue.receive();
		auto request = std::move(req_res.first);
		auto response = std::move(req_res.second);

		response.SetHeader("server", (std::string)_config["server_name"]);
		response.SetHeader("date", GetDate());
		response.SetHeader("connection", "close");

		std::stringstream body_stream;
		auto method = request.GetMethod();
		auto target = request.GetTarget();
		// check method allowed
		ValidateMethod(method, request, response);
		// check route map for requested resource
		auto request_handler = _route_map.GetRouteHandler(method + target).value_or(nullptr);
		if (request_handler)
		{
			request_handler(std::move(request), std::move(response));
			continue;
		}
		target = target == "/" ? "/index.html" : target;
		if (target == "/upload")
		{
			if (method == "POST")
			{
				HandleUpload(std::move(request), std::move(response));
				continue;
			}
			if (method == "GET")
			{
				HandleGetUploads(std::move(request), std::move(response));
				continue;
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
					response.Send();
					continue;
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
					response.Send();
					continue;
				}

				target_file.unsetf(std::ios::skipws);
				std::vector<unsigned char> target_file_contents((std::istream_iterator<char>(target_file)), std::istream_iterator<char>());

				response.SetStatusCode(200);
				response.SetHeader("content-type", "application/octet-stream");
				response.SetHeader("Content-Disposition", R"(inline; filename=")" + filename + R"(")");
				response.SetBody(target_file_contents);
				Log(request, response);
				response.Send();
				continue;
			}
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
			response.Send();
			continue;
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
			response.Send();
			continue;
		}
		std::vector<unsigned char> target_file_contents((std::istreambuf_iterator<char>(target_file)), std::istreambuf_iterator<char>());
		response.SetStatusCode(200);
		response.SetHeader("content-type", "text/html;charset=utf-8");
		response.SetBody(target_file_contents);
		Log(request, response);
		response.Send();
	}
};
void HttpServer::HandleUpload(HttpRequest&& request, HttpResponse&& response)
{
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
		response.Send();
		return;
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
			response.Send();
			return;
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
			response.Send();
			return;
		}
	}
	else if (content_type.find("multipart/form-data") != std::string::npos)
	{
		auto boundary_start = content_type.find("boundary");
		if (boundary_start == std::string::npos)
		{
			response.SetStatusCode(400);
			Log(request, response);
			response.Send();
			return;
		}
		auto boundary_param = std::string(content_type.begin() + boundary_start, content_type.end());
		boundary_start = boundary_param.find("=");
		if (boundary_start == std::string::npos)
		{
			response.SetStatusCode(400);
			Log(request, response);
			response.Send();
			return;
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
			response.Send();
			return;
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
			response.Send();
			return;
		}
		try
		{
			upload_file.write(((char*)body_buffer.data()), body_buffer.size());
		}
		catch (...)
		{
			response.SetStatusCode(500);
			Log(request, response);
			response.Send();
			return;
		}
	}
	response.SetHeader("content-type", "application/json");
	auto api_object = jjson::Object();
	api_object["status"] = "Success!!!";
	api_object["message"] = "Upload Complete";
	response.SetBody(api_object);
	response.SetStatusCode(200);
	Log(request, response);
	response.Send();
	return;
}
void HttpServer::HandleGetUploads(HttpRequest&& request, HttpResponse&& response)
{
	std::stringstream body_stream;
	auto upload_dir = (std::string)_config["upload_dir"];
	if (!fs::exists(fs::path(upload_dir)))
	{
		response.SetHeader("content-type", "text/html;charset=utf-8");
		body_stream << "<body><H1>500 Internal Server Error</H1><div>.</div></body>";
		std::vector<unsigned char> body_vec((std::istreambuf_iterator<char>(body_stream)), std::istreambuf_iterator<char>());
		response.SetBody(body_vec);
		Log(request, response);
		response.Send();
		return;
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
	std::vector<unsigned char> body_vec((std::istreambuf_iterator<char>(body_stream)), std::istreambuf_iterator<char>());
	response.SetBody(body_vec);
	response.SetStatusCode(200);
	Log(request, response);
	response.Send();
	return;
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