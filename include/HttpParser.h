#ifndef __HTTP_PARSER_H_
#define __HTTP_PARSER_H_

#include "Hpack.h"
#include "Http2.h"
#include "HttpMessage.h"
#include "Huffman.h"
#include "RouteMap.h"

#include <memory>
#include <mutex>
#include <vector>

class jSocket;

static bool HasHttp2ConnectionPreface(const std::vector<unsigned char>& data_buffer)
{
	if (data_buffer.size() < 24)
	{
		return false;
	}
	std::string data_buffer_string(data_buffer.begin(), data_buffer.begin() + 24);
	return CONNECTION_PREFACE == data_buffer_string;
}


class HttpParser
{
public:
	HttpParser() = default;
	HttpParser(const std::vector<std::string>& allowed_methods,
			   const std::string& server_name,
			   const std::string& upload_dir,
			   const std::string& web_dir,
			   RouteMap* route_map);

	std::vector<unsigned char> GetConnectionPreface() const;
	std::vector<unsigned char> GetSettingsFrame() const;
	std::vector<unsigned char> GetSettingsFrameWithAck() const;
	std::vector<unsigned char> GetGoAwayFrame(uint32_t last_stream_id, const Http2Error errorCode, std::string debug_info) const;
	HttpResponse HandleHttp2Upgrade();
	HttpResponse HandleHttpRequest(HttpRequest&& request);

	ErrorType HandleHttp2DataFrame(const Http2Frame frame);
	Http2WindowUpdateFrameParseResult HandleHttp2WindowUpdateFrame(const Http2Frame frame);
	ErrorType HandleHttp2HeadersFrame(const Http2Frame frame);
	ErrorType HandleHttp2PriorityFrame(const Http2Frame frame);
	ErrorType HandleHttp2ResetStreamFrame(const Http2Frame frame);
	ErrorType HandleHttp2PushPromiseFrame(const Http2Frame frame);
	ErrorType HandleHttp2PingFrame(const Http2Frame frame);
	ErrorType HandleHttp2GoAwayFrame(const Http2Frame frame);
	ErrorType HandleHttp2ContinuationFrame(const Http2Frame frame);
	std::vector<unsigned char> GetHeadersFrame(uint32_t stream_id,
											   const int status_code,
											   const std::unordered_map<std::string, std::string> headers,
											   uint8_t flags = 0x00);
	std::vector<unsigned char> GetDataFrame(uint32_t stream_id, const std::vector<unsigned char> data_buffer, uint8_t flags = 0x00);

private:
	void Log(const HttpRequest&, const HttpResponse&);
	HttpResponse HandleUpload(HttpRequest&&);
	HttpResponse HandleGetUploads(HttpRequest&&);
	bool ValidateMethod(std::string method)
	{
		std::stringstream body_stream;
		auto method_itr = std::find(_allowed_methods.begin(), _allowed_methods.end(), method);
		return method_itr != _allowed_methods.end();
	}

private:
	std::vector<std::string> _allowed_methods;
	std::mutex _logger_mutex;
	std::string _server_name;
	std::string _upload_dir;
	std::string _web_dir;
	RouteMap* _route_map;
	HPack::Codec _codec;
};
#endif