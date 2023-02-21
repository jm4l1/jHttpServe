#ifndef __HTTP_CONNECTION_H__
#define __HTTP_CONNECTION_H__

#include "HttpMessage.h"
#include "HttpParser.h"
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
	HttpConnection(std::unique_ptr<jSocket> socket, HttpParser* parser);
	~HttpConnection();
	HttpConnection(HttpConnection&& other);
	HttpConnection(HttpConnection& other) = delete;
	void Close();
	void HandleData(const std::vector<unsigned char>& data_buffer);
	inline bool CanClose() const
	{
		return _can_close;
	};
	std::chrono::steady_clock::time_point LastUsedTime();

private:
	void Start();
	std::optional<std::vector<unsigned char>> Receive();
	void Send(const std::vector<unsigned char>& data_buffer);
	void Worker(std::stop_token stop_token);
	inline uint32_t GetNextStreamId()
	{
		return _next_stream_id + 2;
	}

private:
	std::unique_ptr<jSocket> _socket;
	std::chrono::steady_clock::time_point _last_used_time;
	std::mutex _last_used_mutex;
	std::jthread _connection_thread;
	std::atomic<bool> _can_close = false;
	bool _is_http2 = false;
	uint32_t _next_stream_id = 2;
	HttpParser* _parser = nullptr;
};

#endif