#include "HttpServer.h"

#include "Utility.h"

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
	_parser = std::make_unique<HttpParser>(_allowed_methods,
										   static_cast<std::string>(_config["server_name"]),
										   static_cast<std::string>(_config["upload_dir"]),
										   static_cast<std::string>(_config["web_dir"]),
										   &_route_map);

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
		response.SetHeader("date", Utility::GetDate());
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
		std::unique_ptr<HttpConnection> connection = std::make_unique<HttpConnection>(std::move(socket), _parser.get());
		connection->HandleData(request.ToBuffer());
		_connections.emplace_back(std::move(connection));
	}
	std::cout << "[HttpServer] - Application Layer Ending\n";
	_is_server_running = false;
};

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