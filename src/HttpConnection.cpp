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
						auto response = _parser->HandleHttpRequest(std::move(http_request));
						// Http2Frame
						Stream stream(0x01, StreamInitiator::Client);
						auto http2_response_buffer = _parser->HandleHttp2Upgrade().ToBuffer();
						auto http2_settings_frame_buffer = _parser->GetSettingsFrame();
						std::vector<unsigned char> buffer;
						buffer.reserve(http2_response_buffer.size() + http2_settings_frame_buffer.size());
						buffer.insert(buffer.end(), http2_response_buffer.begin(), http2_response_buffer.end());
						buffer.insert(buffer.end(), http2_settings_frame_buffer.begin(), http2_settings_frame_buffer.end());
						_is_http2 = true;
						Send(buffer);
						auto headers_frame_buffer =
							_parser->GetHeadersFrame(0x01, response.GetStatusCode(), response.GetHeaders(), HTTP2_FLAG_HEADERS);
						auto data_frame_buffer = _parser->GetDataFrame(0x01, response.GetBody(), HTTP2_DATA_FLAG_END_STREAM);
						Send(headers_frame_buffer);
						Send(data_frame_buffer);
						return;
					}
				}
				// support further types in the future
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
		size_t offset = 0;
		if (!_received_client_connection_preface && HasHttp2ConnectionPreface(data_buffer))
		{
			_received_client_connection_preface = true;
			offset += HTTP2_MAGIC_PREFACE_LENGTH;
			std::cout << "[HttpConnection] - Connection preface received\n";
		}
		else
		{
			// send connection error
			Send(_parser->GetGoAwayFrame(0x0000, Http2Error::PROTOCOL_ERROR, "Expected connection preface"));
			_can_close = true;
			_connection_thread.request_stop();
			return;
		}
		try
		{
			while (offset < data_buffer.size())
			{
				auto parse_result = Http2Frame::GetFromBuffer(std::vector<unsigned char>(data_buffer.begin() + offset, data_buffer.end()));

				auto error_type_result = std::get_if<ErrorType>(&parse_result);

				if (error_type_result)
				{
					auto last_stream_id = _streams.empty() ? 0x0000 : (_streams.end() - 1)->GetStreamId();
					Send(_parser->GetGoAwayFrame(last_stream_id, error_type_result->_error, error_type_result->_reason));
					_can_close = true;
					_connection_thread.request_stop();
					return;
				}

				auto frame = *(std::get_if<Http2Frame>(&parse_result));

				if (!_received_first_settings_frame)
				{
					if (frame.IsValidSettingsFrame())
					{
						_settings = Http2SettingsFrame(frame.payload).params;
						_received_first_settings_frame = true;
						Send(_parser->GetSettingsFrameWithAck());
					}
					else
					{
						// send connection error
						Send(_parser->GetGoAwayFrame(0x0000, Http2Error::PROTOCOL_ERROR, "Expected settings frame"));
						_can_close = true;
						_connection_thread.request_stop();
						return;
					}
				}

				if (frame.stream_id != 0x0000)
				{
					// we assume we always create the first stream using the upgrade header
					if (_streams.empty() ||
						(frame.type == HTTP2_HEADERS_FRAME && (_streams.end() - 1)->GetStreamId() >= frame.stream_id.to_ulong()))
					{
						Send(_parser->GetGoAwayFrame(0x0000, Http2Error::PROTOCOL_ERROR, "Received unexpected stream Identifier"));
						_can_close = true;
						_connection_thread.request_stop();
						return;
					}

					Stream stream;

					if (frame.type == HTTP2_HEADERS_FRAME)
					{
						// add a new stream
						stream = std::move(Stream(frame.stream_id.to_ulong(), StreamInitiator::Client));
						_streams.emplace_back(std::move(stream));
					}

					else
					{
						auto stream_itr = std::ranges::find_if(_streams,
															   [&frame](const auto& s)
															   {
																   return s.GetStreamId() == frame.stream_id.to_ulong();
															   });
						if (stream_itr == _streams.end())
						{
							Send(_parser->GetGoAwayFrame(0x0000, Http2Error::PROTOCOL_ERROR, "Received unexpected stream Identifier"));
							_can_close = true;
							_connection_thread.request_stop();
							return;
						}
						stream = std::move(*stream_itr);
					}

					auto streamTransistionResult =
						stream.TransitionToNextState(StreamAction::Receive, frame.type.to_ulong(), frame.flags.to_ulong());

					if (streamTransistionResult != Http2Error::NO_ERROR)
					{
						Send(_parser->GetGoAwayFrame((_streams.end() - 1)->GetStreamId(), streamTransistionResult, "Error on stream"));
						_can_close = true;
						_connection_thread.request_stop();
						return;
					}
				}

				if (_expects_continuation &&
					(frame.stream_id.to_ulong() != _stream_awaiting_continuation || frame.type != HTTP2_CONTINUATION_FRAME))
				{
					Send(_parser->GetGoAwayFrame((_streams.end() - 1)->GetStreamId(),
												 Http2Error::PROTOCOL_ERROR,
												 "Expected continuation frame"));
					_can_close = true;
					_connection_thread.request_stop();
					return;
				}

				if (!_expects_continuation && frame.type == HTTP2_CONTINUATION_FRAME)
				{
					Send(_parser->GetGoAwayFrame((_streams.end() - 1)->GetStreamId(),
												 Http2Error::PROTOCOL_ERROR,
												 "Unexpected continuation frame"));
					_can_close = true;
					_connection_thread.request_stop();
					return;
				}

				ErrorType error_type;

				switch (frame.stream_id.to_ulong())
				{
				case HTTP2_WINDOW_UPDATE_FRAME:
				{
					Http2WindowUpdateFrameParseResult result = _parser->HandleHttp2WindowUpdateFrame(frame);
					auto error_type_result = std::get_if<ErrorType>(&result);
					if (error_type_result)
					{
						auto last_stream_id = _streams.empty() ? 0x0000 : (_streams.end() - 1)->GetStreamId();
						Send(_parser->GetGoAwayFrame(last_stream_id, error_type_result->_error, error_type_result->_reason));
						_can_close = true;
						_connection_thread.request_stop();
						return;
					}
					uint32_t window_size_increment = *(std::get_if<uint32_t>(&result));
					auto settings_itr = std::ranges ::find_if(_settings,
															  [](const Http2SettingsParam& param)
															  {
																  return param.identifier.to_ulong() == SETTINGS_INITIAL_WINDOW_SIZE;
															  });
					settings_itr->value = settings_itr->value.to_ulong() + window_size_increment;
					Send(_parser->GetSettingsFrameWithAck());
				}
				case HTTP2_DATA_FRAME:
				{
					error_type = _parser->HandleHttp2DataFrame(frame);
					if (!_expects_continuation && (frame.flags.to_ulong() & HTTP2_DATA_FLAG_END_STREAM != HTTP2_DATA_FLAG_END_STREAM))
					{
					}
					break;
				}
				case HTTP2_HEADERS_FRAME:
				{
					if (frame.flags.to_ulong() & HTTP2_FLAG_HEADERS != HTTP2_FLAG_HEADERS)
					{
						_expects_continuation = true;
						_stream_awaiting_continuation = frame.stream_id.to_ulong();
					}
					error_type = _parser->HandleHttp2HeadersFrame(frame);
					if (!_expects_continuation && (frame.flags.to_ulong() & HTTP2_FLAG_HEADERS != HTTP2_FLAG_HEADERS))
					{
					}
					break;
				}
				case HTTP2_PRIORITY_FRAME:
				{
					error_type = _parser->HandleHttp2PriorityFrame(frame);
					break;
				}
				case HTTP2_RST_STREAM_FRAME:
				{
					error_type = _parser->HandleHttp2ResetStreamFrame(frame);
					break;
				}
				case HTTP2_PING_FRAME:
				{
					error_type = _parser->HandleHttp2PingFrame(frame);
					break;
				}
				case HTTP2_GOAWAY_FRAME:
				{
					error_type = _parser->HandleHttp2GoAwayFrame(frame);
					break;
				}
				case HTTP2_CONTINUATION_FRAME:
				{
					error_type = _parser->HandleHttp2ContinuationFrame(frame);
					break;
				}

				default:
				{
					error_type._error = Http2Error::PROTOCOL_ERROR;
					error_type._reason = "Unsupported frame type received";
					return;
				}
				}

				if (error_type._error != Http2Error::NO_ERROR)
				{
					auto last_stream_id = _streams.empty() ? 0x0000 : (_streams.end() - 1)->GetStreamId();
					Send(_parser->GetGoAwayFrame(last_stream_id, error_type._error, error_type._reason));
					_can_close = true;
					_connection_thread.request_stop();
					return;
				}
				offset += frame.length.to_ulong() + HTTP2_FRAME_HEADER_LENGTH;
			}
			// all frame processed
		}
		catch (const std::length_error& e)
		{
			std::cout << "[HttpConnection] - Caught an error - " << e.what() << "\n ";
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