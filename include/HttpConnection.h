#ifndef __HTTP_CONNECTION_H__
#define __HTTP_CONNECTION_H__

#include "HttpMessage.h"
#include "jSocket.h"

#include <chrono>
#include <functional>
#include <optional>
#include <stop_token>
#include <thread>
#include <vector>

class HttpConnection
{
public:
	HttpConnection(std::unique_ptr<jSocket> socket);
	HttpConnection(HttpConnection&& other);
	HttpConnection(HttpConnection& other) = delete;
	std::chrono::steady_clock::time_point LastReceivedTime() const;
	void Close();
	void SetDataHandler(const std::function<std::optional<HttpResponse>(const std::vector<unsigned char>&)>& data_handler);
	void HandleData(const std::vector<unsigned char>& data_buffer);
	inline bool CanClose() const
	{
		return _can_close;
	};

private:
	void Start();
	std::optional<std::vector<unsigned char> > Receive();
	void Send(const std::vector<unsigned char>& data_buffer);
	void Worker(std::stop_token stop_token);

private:
	std::unique_ptr<jSocket> _socket;
	std::chrono::steady_clock::time_point _last_received_time;
	std::jthread _connection_thread;
	std::function<std::optional<HttpResponse>(const std::vector<unsigned char>)> _data_handler = nullptr;
	std::atomic<bool> _can_close = false;
};

#endif