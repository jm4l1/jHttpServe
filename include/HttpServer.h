#ifndef _HTTPSERVER_H_
#define _HTTPSERVER_H_

#include "HttpConnection.h"
#include "HttpMessage.h"
#include "MessageQueue.h"
#include "RouteMap.h"
#include "SocketServer.h"
#include "jSocket.h"
#include "jjson.hpp"

#include <atomic>
#include <chrono>
#include <iomanip>
#include <iterator>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>

constexpr std::chrono::seconds CONNECTION_TIMEOUT(5);
class HttpServer
{
public:
	HttpServer(){};
	~HttpServer(){};
	void Init(std::string);
	void Get(std::string, std::function<HttpResponse(HttpRequest&&)>);
	void Post(std::string, std::function<HttpResponse(HttpRequest&&)>);

private:
	void ParseConfigFile(std::string);
	void HandleApplicationLayerHttp2Sync(HttpRequest&&, HttpResponse&&);
	void HandleApplicationLayer(std::stop_token stop_token);
	HttpResponse HandleUpload(HttpRequest&&);
	HttpResponse HandleGetUploads(HttpRequest&&);
	void Log(const HttpRequest&, const HttpResponse&);
	bool ValidateMethod(std::string method)
	{
		std::stringstream body_stream;
		auto method_itr = std::find(_allowed_methods.begin(), _allowed_methods.end(), method);
		return method_itr != _allowed_methods.end();
	}
	static std::string GetDate()
	{
		auto now = std::chrono::system_clock::now();
		auto date = std::chrono::system_clock::to_time_t(now);
		std::stringstream date_stream;
		date_stream << std::put_time(std::gmtime(&date), "%a, %d %b %Y %OH:%M:%S GMT");
		return date_stream.str();
	};
	static std::string GetshortDate()
	{
		auto now = std::chrono::system_clock::now();
		auto date = std::chrono::system_clock::to_time_t(now);
		std::stringstream date_stream;
		date_stream << std::put_time(std::localtime(&date), "%Y%m%d_%OH:%M:%S");
		return date_stream.str();
	};
	void PerformSocketTask(std::stop_token stop_token);
	void ParseData(std::vector<unsigned char>&& message_buffer, std::unique_ptr<jSocket> socket);
	HttpResponse HandleHttpRequest(HttpRequest&& request);

private:
	jjson::value _config;
	MessageQueue<std::pair<HttpRequest, std::unique_ptr<jSocket>>> _request_queue;
	jSocket _server_socket;
	RouteMap _route_map;
	std::mutex _logger_mutex;
	std::vector<std::string> _allowed_methods;
	std::jthread _socket_thread;
	std::jthread _app_logic_thread;
	std::vector<std::unique_ptr<HttpConnection>> _connections = {};
	std::atomic<bool> _is_server_running = true;
	std::condition_variable _application_state_cond_var;
};

#endif