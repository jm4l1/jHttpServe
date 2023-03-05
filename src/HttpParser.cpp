#include "HttpParser.h"

#include "Utility.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <ranges>
#include <utility>

namespace fs = std::filesystem;

HttpParser::HttpParser(const std::vector<std::string>& allowed_methods,
					   const std::string& server_name,
					   const std::string& upload_dir,
					   const std::string& web_dir,
					   RouteMap* route_map)
  : _allowed_methods(allowed_methods)
  , _server_name(server_name)
  , _upload_dir(upload_dir)
  , _web_dir(web_dir)
  , _route_map(route_map)
{
}
void HttpParser::Log(const HttpRequest& request, const HttpResponse& response)
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

HttpResponse HttpParser::HandleHttp2Upgrade()
{
	HttpResponse response;
	response.SetStatusCode(101);
	response.SetHeader("Connection", "Upgrade");
	response.SetHeader("Upgrade", "h2c");
	response.SetHeader("server", _server_name);
	response.SetHeader("date", Utility::GetDate());

	// Log(request, response);
	return response;
};

std::vector<unsigned char> HttpParser::GetConnectionPreface() const
{
	std::vector<unsigned char> connection_preface(CONNECTION_PREFACE, (CONNECTION_PREFACE) + strlen(CONNECTION_PREFACE));
	return connection_preface;
}
std::vector<unsigned char> HttpParser::GetSettingsFrame() const
{
	auto settings_max_frame_size = Http2SettingsParam(SETTINGS_MAX_FRAME_SIZE, 16777215);
	auto settings_initial_window_size = Http2SettingsParam(SETTINGS_INITIAL_WINDOW_SIZE, 1048576);
	auto settings_max_concurrent_streams = Http2SettingsParam(SETTINGS_MAX_CONCURRENT_STREAMS, 100);
	auto settings_frame = Http2SettingsFrame();

	settings_frame.AddParam(settings_max_frame_size);
	settings_frame.AddParam(settings_initial_window_size);
	settings_frame.AddParam(settings_max_concurrent_streams);

	auto frame = Http2Frame();
	frame.type = HTTP2_SETTINGS_FRAME;
	frame.length = settings_frame.Size();
	frame.flags = 0x0;
	frame.stream_id = 0x0;
	frame.payload = settings_frame.Serialize();

	return frame.Serialize();
}

std::vector<unsigned char> HttpParser::GetGoAwayFrame(uint32_t last_stream_id, const Http2Error errorCode, std::string debug_info) const
{
	auto frame = Http2Frame();
	frame.type = HTTP2_SETTINGS_FRAME;
	frame.flags = 0x1;
	frame.stream_id = 0x0;
	frame.length = sizeof(last_stream_id) + sizeof(errorCode) + debug_info.size();
	frame.payload.reserve(frame.length.to_ulong());
	size_t offset = 0;
	memcpy(frame.payload.data() + offset, &last_stream_id, sizeof(last_stream_id));
	offset += sizeof(last_stream_id);
	memcpy(frame.payload.data() + offset, &errorCode, sizeof(errorCode));
	offset += sizeof(errorCode);
	memcpy(frame.payload.data() + offset, debug_info.data(), debug_info.size());
	return frame.Serialize();
}

std::vector<unsigned char> HttpParser::GetHeadersFrame(uint32_t stream_id,
													   const int status_code,
													   const std::unordered_map<std::string, std::string> headers,
													   uint8_t flags)
{
	auto frame = Http2Frame();
	frame.type = HTTP2_HEADERS_FRAME;
	frame.flags = flags;
	frame.stream_id = stream_id;
	size_t offset = 0;
	auto header_buffer = _codec.Encode(":status", std::to_string(status_code));
	frame.payload.reserve(offset + header_buffer.size());
	frame.payload.insert(frame.payload.end(), header_buffer.begin(), header_buffer.end());
	offset += header_buffer.size();
	std::ranges::for_each(headers,
						  [&frame, &offset, this](const auto& header)
						  {
							  auto header_buffer = _codec.Encode(header.first, header.second);
							  frame.payload.reserve(offset + header_buffer.size());
							  frame.payload.insert(frame.payload.end(), header_buffer.begin(), header_buffer.end());
							  offset += header_buffer.size();
						  });
	frame.length = frame.payload.size();
	return frame.Serialize();
}

std::vector<unsigned char> HttpParser::GetDataFrame(uint32_t stream_id, const std::vector<unsigned char> data_buffer, uint8_t flags)
{
	auto frame = Http2Frame();
	frame.type = HTTP2_DATA_FRAME;
	frame.flags = flags;
	frame.stream_id = stream_id;
	frame.length = data_buffer.size();
	frame.payload.reserve(frame.length.to_ulong());
	frame.payload.insert(frame.payload.begin(), data_buffer.begin(), data_buffer.end());
	return frame.Serialize();
}
std::vector<unsigned char> HttpParser::GetSettingsFrameWithAck() const
{
	auto settings_frame = Http2SettingsFrame();
	auto frame = Http2Frame();
	frame.type = HTTP2_SETTINGS_FRAME;
	frame.length = 0;
	frame.flags = 0x1;
	frame.stream_id = 0x0;

	return frame.Serialize();
}

HttpResponse HttpParser::HandleHttpRequest(HttpRequest&& request)
{
	HttpResponse response = HttpResponse();
	response.SetHeader("server", _server_name);
	response.SetHeader("date", Utility::GetDate());
	response.SetHeader("connection", request.GetHeader("Connection").value_or("close"));
	std::stringstream body_stream;
	auto method = request.GetMethod();
	auto target = request.GetTarget();

	// check method allowed
	if (!ValidateMethod(method))
	{
		std::cout << "[HttpServer] - Http Validation failed for method " << method << "\n";
		response.SetStatusCode(405);
		response.SetHeader("connection", "close");
		std::stringstream allowed_stream;
		std::ostream_iterator<std::string> outputString(allowed_stream, ",");
		std::copy(_allowed_methods.begin(), _allowed_methods.end(), outputString);
		response.SetHeader("allow", allowed_stream.str());
		body_stream << "<body><div><H1>405 Method Not Allowed</H1>" << allowed_stream.str() << "</div></body>";
		response.SetBody(body_stream.str());
		Log(request, response);
		return response;
	}

	// check route map for requested resource
	if (_route_map)
	{
		auto request_handler = _route_map->GetRouteHandler(method + target).value_or(nullptr);
		if (request_handler)
		{
			return request_handler(std::move(request));
		}
	}
	target = target == "/" ? "/index.html" : target;
	if (target == "/upload")
	{
		if (method == "POST")
		{
			return HandleUpload(std::move(request));
		}
		if (method == "GET")
		{
			return HandleGetUploads(std::move(request));
		}
	}
	if (auto pos = target.find("/upload/") != std::string::npos)
	{
		if (method == "GET")
		{
			auto filename = std::string(target.begin() + 8, target.end());
			auto file_location = _upload_dir + "/" + filename;
			if (!fs::exists(fs::path(file_location)))
			{
				response.SetStatusCode(404);
				response.SetHeader("content-type", "text/html;charset=utf-8");
				body_stream << "<body><div><H1>404 Not Found</H1>" << filename << " not found.</div></body>";
				std::vector<unsigned char> body_vec((std::istreambuf_iterator<char>(body_stream)), std::istreambuf_iterator<char>());
				response.SetBody(body_vec);
				Log(request, response);
				return response;
			}
			std::ifstream target_file(file_location, std::ios::binary);
			if (!target_file.is_open())
			{
				std::cout << "[HttpParser] - got error opening file " << file_location << "\n";
				response.SetStatusCode(500);
				response.SetHeader("content-type", "text/html;charset=utf-8");
				body_stream << "<body><H1>500 Internal Server Error</H1><div>.</div></body>";
				std::vector<unsigned char> body_vec((std::istreambuf_iterator<char>(body_stream)), std::istreambuf_iterator<char>());
				response.SetBody(body_vec);
				Log(request, response);
				return response;
			}

			target_file.unsetf(std::ios::skipws);
			std::vector<unsigned char> target_file_contents((std::istream_iterator<char>(target_file)), std::istream_iterator<char>());

			response.SetStatusCode(200);
			response.SetHeader("content-type", "application/octet-stream");
			response.SetHeader("Content-Disposition", R"(inline; filename=")" + filename + R"(")");
			response.SetBody(target_file_contents);
			Log(request, response);
			return response;
		}

		response.SetStatusCode(405);
		response.SetHeader("allow", "GET");
		body_stream << "<body><div><H1>405 Method Not Allowed</H1><p>The request method " << method << " is not appropriate for the target "
					<< target << ".</p></div></body>";
		response.SetBody(body_stream.str());
		Log(request, response);
		return response;
	}
	// fall back to web dir
	auto target_location = _web_dir + "/" + target;
	if (!fs::exists(fs::path(target_location)))
	{
		response.SetStatusCode(404);
		response.SetHeader("content-type", "text/html;charset=utf-8");
		body_stream << "<body><div><H1>404 Not Found</H1>" << target << " not found.</div></body>";
		std::vector<unsigned char> body_vec((std::istreambuf_iterator<char>(body_stream)), std::istreambuf_iterator<char>());
		response.SetBody(body_vec);
		Log(request, response);
		return response;
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
		return response;
	}
	std::vector<unsigned char> target_file_contents((std::istreambuf_iterator<char>(target_file)), std::istreambuf_iterator<char>());
	response.SetStatusCode(200);
	response.SetHeader("content-type", "text/html;charset=utf-8");
	response.SetBody(target_file_contents);
	Log(request, response);
	return response;
};

HttpResponse HttpParser::HandleUpload(HttpRequest&& request)
{
	HttpResponse response;
	response.SetHeader("connection", request.GetHeader("Connection").value_or("close"));
	response.SetHeader("server", _server_name);
	response.SetHeader("date", Utility::GetDate());
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
		return response;
	}
	if (!fs::exists(fs::path(_upload_dir)))
	{
		fs::create_directory((std::string)_upload_dir);
	}
	if (content_type.find("text/plain") != std::string::npos)
	{
		auto file_location = _upload_dir + "/" + "file" + Utility::GetshortDate();
		auto upload_file = std::ofstream(file_location, std::ios::binary);
		if (!upload_file)
		{
			std::cout << "[HttpParser] - got error opening file " << file_location << "\n";
			response.SetStatusCode(500);
			Log(request, response);
			return response;
		}
		try
		{
			auto body_buffer = request.GetBody();
			upload_file.write(((char*)body_buffer.data()), body_buffer.size());
		}
		catch (const std::exception& e)
		{
			std::cout << "[HttpParser] - got error writing to upload dir (" << e.what() << ")\n";
			response.SetStatusCode(500);
			Log(request, response);
			return response;
		}
	}
	else if (content_type.find("multipart/form-data") != std::string::npos)
	{
		auto boundary_start = content_type.find("boundary");
		if (boundary_start == std::string::npos)
		{
			response.SetStatusCode(400);
			Log(request, response);
			return response;
		}
		auto boundary_param = std::string(content_type.begin() + boundary_start, content_type.end());
		boundary_start = boundary_param.find("=");
		if (boundary_start == std::string::npos)
		{
			response.SetStatusCode(400);
			Log(request, response);
			return response;
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
			return response;
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
			file_name = "file" + Utility::GetshortDate();
		}
		auto file_location = _upload_dir + "/" + file_name;
		auto upload_file = std::ofstream(file_location, std::ios::binary);
		if (!upload_file)
		{
			response.SetStatusCode(500);
			Log(request, response);
			return response;
		}
		try
		{
			upload_file.write(((char*)body_buffer.data()), body_buffer.size());
		}
		catch (...)
		{
			response.SetStatusCode(500);
			Log(request, response);
			return response;
		}
	}
	response.SetHeader("content-type", "application/json");
	auto api_object = jjson::Object();
	api_object["status"] = "Success!!!";
	api_object["message"] = "Upload Complete";
	response.SetBody(api_object);
	response.SetStatusCode(200);
	Log(request, response);
	return response;
}

HttpResponse HttpParser::HandleGetUploads(HttpRequest&& request)
{
	HttpResponse response;
	std::stringstream body_stream;
	auto upload_dir = (std::string)_upload_dir;
	if (!fs::exists(fs::path(upload_dir)))
	{
		std::cout << "[HttpParser] - Upload directory not found" << upload_dir << "\n";
		response.SetHeader("content-type", "text/html;charset=utf-8");
		body_stream << "<body><H1>500 Internal Server Error</H1><div>.</div></body>";
		std::vector<unsigned char> body_vec((std::istreambuf_iterator<char>(body_stream)), std::istreambuf_iterator<char>());
		response.SetBody(body_vec);
		Log(request, response);
		return response;
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
	response.SetHeader("server", _server_name);
	response.SetHeader("date", Utility::GetDate());
	response.SetHeader("connection", request.GetHeader("Connection").value_or("close"));
	std::vector<unsigned char> body_vec((std::istreambuf_iterator<char>(body_stream)), std::istreambuf_iterator<char>());
	response.SetBody(body_vec);
	response.SetStatusCode(200);
	Log(request, response);
	return response;
}

Http2WindowUpdateFrameParseResult HttpParser::HandleHttp2WindowUpdateFrame(const Http2Frame frame)
{
	uint32_t window_size_increment = 0;
	memcpy(&window_size_increment, frame.payload.data(), sizeof(window_size_increment));
	if (window_size_increment == 0)
	{
		return ErrorType({ ._error = Http2Error::PROTOCOL_ERROR, ._reason = "Invalid window size increment" });
	}
	return window_size_increment;
}
ErrorType HttpParser::HandleHttp2DataFrame(const Http2Frame frame)
{
	ErrorType error_type = { ._error = Http2Error::NO_ERROR, ._reason = "" };
	return error_type;
}
ErrorType HttpParser::HandleHttp2HeadersFrame(const Http2Frame frame)
{
	ErrorType error_type = { ._error = Http2Error::NO_ERROR, ._reason = "" };
	bool has_priority = (frame.flags.to_ulong() & HTTP2_HEADERS_FLAG_PRIORITY) | HTTP2_HEADERS_FLAG_PRIORITY;
	size_t offset = 0;

	uint8_t pad_length = 0x00;
	uint32_t stream_id;
	uint8_t weight;

	if ((frame.flags.to_ulong() & HTTP2_FLAG_PADDED) | HTTP2_FLAG_PADDED)
	{
		memcpy(&pad_length, &frame.payload + offset, sizeof(weight));
		if (pad_length >= frame.payload.size())
		{
			error_type._error = Http2Error::PROTOCOL_ERROR;
			error_type._reason = "Invalid padded length";
			return error_type;
		}
		offset += sizeof(weight);
	}

	memcpy(&stream_id, &frame.payload + offset, sizeof(stream_id));

	if (has_priority)
	{
		stream_id ^= 0x8000;
		memcpy(&weight, &frame.payload + offset, sizeof(weight));
		offset += sizeof(weight);
	}
	std::vector<HPack::TableEntry> header_list;
	_codec.Decode(std::vector<uint8_t>(frame.payload.begin() + offset, frame.payload.begin() + offset + frame.length.to_ulong()),
				  header_list);
	return error_type;
}
ErrorType HttpParser::HandleHttp2PriorityFrame(const Http2Frame frame)
{
	ErrorType error_type = { ._error = Http2Error::NO_ERROR, ._reason = "" };
	return error_type;
}
ErrorType HttpParser::HandleHttp2ResetStreamFrame(const Http2Frame frame)
{
	ErrorType error_type = { ._error = Http2Error::NO_ERROR, ._reason = "" };
	return error_type;
}
ErrorType HttpParser::HandleHttp2PushPromiseFrame(const Http2Frame frame)
{
	ErrorType error_type = { ._error = Http2Error::NO_ERROR, ._reason = "" };
	return error_type;
}
ErrorType HttpParser::HandleHttp2PingFrame(const Http2Frame frame)
{
	ErrorType error_type = { ._error = Http2Error::NO_ERROR, ._reason = "" };
	return error_type;
}
ErrorType HttpParser::HandleHttp2GoAwayFrame(const Http2Frame frame)
{
	ErrorType error_type = { ._error = Http2Error::NO_ERROR, ._reason = "" };
	return error_type;
}
ErrorType HttpParser::HandleHttp2ContinuationFrame(const Http2Frame frame)
{
	ErrorType error_type = { ._error = Http2Error::NO_ERROR, ._reason = "" };
	return error_type;
}