#include "HttpConnection.h"

#include <algorithm>
#include <ranges>
#include <utility>

HttpConnection::HttpConnection(std::unique_ptr<jSocket> socket, HttpParser* parser)
  : _socket(std::move(socket))
  , _parser(parser)
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

void HttpConnection::HandleData(const std::vector<unsigned char>& data_buffer)
{
	// http/1.*
	{
		HttpRequest http_request(data_buffer);
		if (http_request.isValid)
		{
			if (http_request.GetHeader("Upgrade").has_value())
			{
				auto upgrade_token = http_request.GetHeader("Upgrade").value();
				// http2 upgrade
				if (upgrade_token == "h2c")
				{
					if (http_request.GetHeader("HTTP2-Settings").has_value())
					{
						auto http2_response_buffer = _parser->HandleHttp2Upgrade(std::move(http_request)).ToBuffer();
						auto http2_settings_frame_buffer = _parser->GetSettingsFrame();
						std::vector<unsigned char> buffer;
						buffer.reserve(http2_response_buffer.size() + http2_settings_frame_buffer.size());
						buffer.insert(buffer.end(), http2_response_buffer.begin(), http2_response_buffer.end());
						buffer.insert(buffer.end(), http2_settings_frame_buffer.begin(), http2_settings_frame_buffer.end());
						_is_http2 = true;
						Send(buffer);
						return;
					}
				}
			}

			auto response = _parser->HandleHttpRequest(std::move(http_request));
			auto connectionHeaderLower = response.GetHeader("connection").value_or("");
			auto connectionHeaderUpper = response.GetHeader("Connection").value_or("");

			if (connectionHeaderLower == "Close" || connectionHeaderLower == "close" || connectionHeaderUpper == "Close" ||
				connectionHeaderUpper == "close")
			{
				_can_close = true;
			}
			Send(response.ToBuffer());
			return;
		}
	}
	// http/2.0
	if (_is_http2)
	{
		if (!_received_client_connection_preface && HasHttp2ConnectionPreface(data_buffer))
		{
			std::cout << "received connection preface\n";
			_received_client_connection_preface = true;
		}
		else
		{
			std::cout << "expected connection preface but got " << data_buffer.size() << "bytes of \n\""
					  << std::string(data_buffer.begin(), data_buffer.end()) << "\" instead of \"\n"
					  << CONNECTION_PREFACE << "\"\n";
			// send connection error
			return;
		}
		try
		{
			size_t offset = 24;
			while (offset < data_buffer.size())
			{
				auto frame = Http2Frame::GetFromBuffer(std::vector<unsigned char>(data_buffer.begin() + offset, data_buffer.end()));
				if (!_received_first_settings_frame)
				{
					if (frame.IsValidSettingsFrame())
					{
						_settings = Http2SettingsFrame(frame.payload).params;
						_received_first_settings_frame = true;
						std::cout << "got first settings frame\n";
						Send(_parser->GetSettingsFrameWithAck());
					}
					else
					{
						// send connection error
						std::cout << "Expected settings frame\n";
						return;
					}
				}
				std::cout << "got a frame of " << frame.length.to_ulong() + 9 << " bytes\n";
				offset += frame.length.to_ulong() + 9;
			}
		}
		catch (const std::length_error& e)
		{
			std::cout << "caught an error - " << e.what() << "\n ";
		}
	}
	std::cout << "[HttpServer] - Unknown data received " << data_buffer.size() << "\n";
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