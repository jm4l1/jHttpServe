#include "HttpConnection.h"

#include <utility>


HttpConnection::HttpConnection(std::unique_ptr<jSocket> socket)
  : _socket(std::move(socket))
{
	Start();
}

HttpConnection::~HttpConnection()
{
	if (_connection_thread.joinable())
	{
		_connection_thread.request_stop();
	}
	if (_socket)
	{
		_socket->Close();
	}
}
HttpConnection::HttpConnection(HttpConnection&& other)
{
	this->_socket = std::move(other._socket);
	this->_connection_thread = std::move(other._connection_thread);
	this->_data_handler = std::move(other._data_handler);
	Start();
}

void HttpConnection::Close()
{
	_connection_thread.request_stop();
}
void HttpConnection::Start()
{
	_connection_thread = std::jthread(std::bind_front(&HttpConnection::Worker, this));
}

std::optional<std::vector<unsigned char>> HttpConnection::Receive()
{
	auto read_result = _socket->Read();
	auto read_error = std::get_if<ReadError>(&read_result);
	if (read_error)
	{
		switch (*read_error)
		{
		case ReadError::ConnectionClosed:
			throw std::runtime_error("Socket closed by peer");
			break;

		case ReadError::TimeOut:
			return std::nullopt;
			break;

		default:
			throw std::runtime_error("Error reading");
			break;
		}
	}
	return *(std::get_if<std::vector<unsigned char>>(&read_result));
}

void HttpConnection::SetDataHandler(const std::function<std::optional<HttpResponse>(const std::vector<unsigned char>&)>& data_handler)
{
	_data_handler = data_handler;
}
void HttpConnection::HandleData(const std::vector<unsigned char>& data_buffer)
{
	if (_data_handler)
	{
		auto response = _data_handler(data_buffer);
		if (response.has_value())
		{
			auto connectionHeaderLower = response.value().GetHeader("connection").value_or("");
			auto connectionHeaderUpper = response.value().GetHeader("Connection").value_or("");

			if (connectionHeaderLower == "Close" || connectionHeaderLower == "close" || connectionHeaderUpper == "Close" ||
				connectionHeaderUpper == "close")
			{
				_can_close = true;
			}
			Send(response.value().ToBuffer());
		}
	}
}

void HttpConnection::Send(const std::vector<unsigned char>& data_buffer)
{
	_socket->Write(data_buffer);
	std::unique_lock lock(_last_used_mutex);
	_last_used_time = std::chrono::steady_clock::now();
}

void HttpConnection::Worker(std::stop_token stop_token)
{
	try
	{
		while (!stop_token.stop_requested())
		{
			auto read_buffer = Receive();
			if (read_buffer.has_value())
			{
				HandleData(read_buffer.value());
				std::unique_lock lock(_last_used_mutex);
				_last_used_time = std::chrono::steady_clock::now();
			}
		}
	}
	catch (const std::exception& e)
	{
		std::cout << "[Http Connection] - caught an exception (" << e.what() << ")\n";
	}
}

std::chrono::steady_clock::time_point HttpConnection::LastUsedTime()
{
	std::unique_lock lock(_last_used_mutex);
	return _last_used_time;
}