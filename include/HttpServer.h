#ifndef _HTTPSERVER_H_
#define _HTTPSERVER_H_

#include "Http2.h"
#include "HttpConnection.h"
#include "HttpMessage.h"
#include "HttpParser.h"
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
	void Log(const HttpRequest&, const HttpResponse&);
	void ParseConfigFile(std::string);
	void HandleApplicationLayer(std::stop_token stop_token);
	void PerformSocketTask(std::stop_token stop_token);
	void ParseData(std::vector<unsigned char>&& message_buffer, std::unique_ptr<jSocket> socket);


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
	std::unique_ptr<HttpParser> _parser = nullptr;
};

#endif