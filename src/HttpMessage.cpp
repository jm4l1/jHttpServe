#include "HttpMessage.h"

#include <iostream>
#include <iterator>
#include <sstream>

std::string HttpMessage::ToString() const
{
	std::stringstream http_stream;
	http_stream << GetStartLine() << CR << LF;
	for (auto header : _headers)
	{
		http_stream << header.first << ": " << header.second << CR << LF;
	}
	http_stream << CR << LF;

	if (_body.size() > 0)
	{
		http_stream << _body.data();
	}

	return http_stream.str().data();
};

std::vector<unsigned char> HttpMessage::ToBuffer() const
{
	std::vector<unsigned char> message_buffer;
	auto start_line = GetStartLine();
	message_buffer.insert(message_buffer.end(), start_line.begin(), start_line.end());
	message_buffer.insert(message_buffer.end(), CR);
	message_buffer.insert(message_buffer.end(), LF);
	for (auto header : _headers)
	{
		message_buffer.insert(message_buffer.end(), header.first.begin(), header.first.end());
		message_buffer.insert(message_buffer.end(), ':');
		message_buffer.insert(message_buffer.end(), ' ');
		message_buffer.insert(message_buffer.end(), header.second.begin(), header.second.end());
		message_buffer.insert(message_buffer.end(), CR);
		message_buffer.insert(message_buffer.end(), LF);
	}
	message_buffer.insert(message_buffer.end(), CR);
	message_buffer.insert(message_buffer.end(), LF);

	if (_body.size() > 0)
	{
		message_buffer.insert(message_buffer.end(), _body.begin(), _body.end());
	}

	return message_buffer;
};

void HttpMessage::SetHeader(std::string header_name, std::string header_value)
{
	_headers[header_name] = header_value;
};

void HttpMessage::SetBody(const std::vector<unsigned char>& body)
{
	_body = body;
	auto body_length_char = _body.size();
	auto body_length_bytes = body_length_char * sizeof(_body[0]);
	SetHeader("content-length", std::to_string(body_length_bytes));
};

void HttpMessage::SetBody(const jjson::value& json_body)
{
	auto json_buffer = json_body.to_string().c_str();
	auto json_buffer_size = json_body.to_string().size() * sizeof(char);
	_body = std::vector<unsigned char>(json_buffer, json_buffer + json_buffer_size);
	SetHeader("content-length", std::to_string(json_buffer_size));
};

std::optional<std::string> HttpMessage::GetHeader(std::string header_name) const
{
	auto kv_pair = _headers.find(header_name);
	if (kv_pair == _headers.end())
	{
		return std::nullopt;
	}
	return kv_pair->second;
};

std::vector<unsigned char> HttpMessage::GetBody() const
{
	return _body;
};

void HttpMessage::SetVersion(std::string version)
{
	_http_version = version;
};

HttpRequest::HttpRequest(std::vector<unsigned char> request_buffer)
{
	auto line_end = std::find(request_buffer.begin(), request_buffer.end(), '\r');
	if (line_end == request_buffer.end())
	{
		isValid = false;
		return;
	}

	// status line
	std::stringstream status_line_stream;
	std::for_each(request_buffer.begin(),
				  line_end,
				  [&](auto c)
				  {
					  status_line_stream << c;
				  });
	status_line_stream >> _method >> _request_target >> _http_version;
	if (std::find(Methods.begin(), Methods.end(), _method) == Methods.end())
	{
		isValid = false;
		return;
	}
	request_buffer.erase(request_buffer.begin(), line_end + 2);
	line_end = std::find(request_buffer.begin(), request_buffer.end(), '\n');
	// headers
	while (line_end != request_buffer.end())
	{
		std::stringstream header_line_stream;
		std::string header_name, header_value;
		std::for_each(request_buffer.begin(),
					  line_end,
					  [&](auto c)
					  {
						  header_line_stream << c;
					  });
		std::getline(header_line_stream, header_name, ':');
		std::getline(header_line_stream, header_value, '\r');
		SetHeader(header_name, header_value);
		request_buffer.erase(request_buffer.begin(), line_end + 2);
		auto next_char = request_buffer[0];
		if (next_char == '\r')
		{
			request_buffer.erase(request_buffer.begin(), request_buffer.begin() + 2);
			break;
		}
		line_end = std::find(request_buffer.begin(), request_buffer.end(), '\r');
	}
	// body
	SetBody(request_buffer);
	isValid = true;
};

void HttpRequest::SetMethod(std::string method)
{
	_method = method;
}

void HttpRequest::SetTarget(std::string target)
{
	_request_target = target;
};

std::string HttpRequest::GetStartLine() const
{
	std::stringstream start_line_stream;
	start_line_stream << _method << " " << _request_target << " " << _http_version;
	return start_line_stream.str();
};

HttpResponse::HttpResponse(std::string response_string)
{
	auto request_stream = std::stringstream(response_string);
	std::string stream_line;

	// status line
	std::getline(request_stream, stream_line);
	std::istringstream status_line_stream(stream_line);
	status_line_stream >> _http_version >> _status_code >> _reason_phrase;
	std::getline(request_stream, stream_line);

	// headers
	while (std::getline(request_stream, stream_line) && (stream_line != ""))
	{
		std::istringstream header_line_stream(stream_line);
		std::string header_name, header_value;
		std::getline(header_line_stream, header_name, ':');
		header_line_stream >> header_value;
		SetHeader(header_name, header_value);
	}
	// body
	std::string body_string;
	while (std::getline(request_stream, stream_line))
	{
		body_string.append(stream_line).append("\n");
	}
	SetBody(body_string);
};

void HttpResponse::SetStatusCode(int status_code)
{
	_status_code = status_code;
	_reason_phrase = ResponseCodes[status_code];
};

void HttpResponse::SetReasonPhrase(std::string reason_phrase)
{
	_reason_phrase = reason_phrase;
};

std::string HttpResponse::GetStartLine() const
{
	std::stringstream start_line_stream;
	start_line_stream << _http_version << " " << _status_code << " " << _reason_phrase;
	return start_line_stream.str();
};