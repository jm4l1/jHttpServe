#ifndef __HTTP_MSSAGE_H__
#define __HTTP_MSSAGE_H__

#include "Http2.h"
#include "jjson.hpp"

#include <future>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#define SP char(32)
#define CR char(13)
#define LF char(10)

static std::vector<std::string> Methods({ "GET", "HEAD", "POST", "PUT", "DELETE", "CONNECT", "OPTIONS", "TRACE" });

static std::unordered_map<int, std::string> ResponseCodes = { { 101, "Switching Protocols" },
															  { 200, "OK" },
															  { 400, "Bad Request" },
															  { 401, "Unauthorized" },
															  { 403, "Forbidden" },
															  { 404, "Not Found" },
															  { 405, "Method Not Allowed" },
															  { 415, "Unsupported Media Type" },
															  { 500, "Internal Server Error" },
															  { 501, "Not Implemented" },
															  { 502, "Bad Gateway" },
															  { 503, "Service Unavailable" },
															  { 504, "Gateway Timeout" },
															  { 505, "HTTP Version Not Supported" } };
class HttpMessage
{
public:
	HttpMessage() = default;
	virtual ~HttpMessage(){};
	std::string ToString() const;
	std::vector<unsigned char> ToBuffer() const;
	void SetHeader(std::string, std::string);
	void SetBody(const jjson::value&);
	void SetBody(const std::vector<unsigned char>&);
	void AppendToBody(const std::vector<unsigned char>& buffer);
	std::optional<std::string> GetHeader(std::string) const;
	std::vector<unsigned char> GetBody() const;
	void SetVersion(std::string);
	virtual std::string GetStartLine() const
	{
		return "";
	};

protected:
	std::unordered_map<std::string, std::string> _headers;
	std::vector<unsigned char> _body;
	std::string _http_version = "HTTP/1.1";
};
class HttpRequest : public HttpMessage
{
public:
	HttpRequest()
	  : HttpMessage(){};
	HttpRequest(std::string);
	HttpRequest(std::vector<unsigned char>);
	HttpRequest(const HttpRequest& B)
	{
		this->_method = B._method;
		this->_headers = B._headers;
		this->_body = B._body;
		this->_http_version = B._http_version;
		this->_request_target = B._request_target;
		this->isValid = B.isValid;
	};
	HttpRequest(HttpRequest&& B)
	{
		this->_method = B._method;
		this->_request_target = B._request_target;
		this->isValid = B.isValid;
		this->_headers = B._headers;
		this->_body = B._body;
		this->_http_version = B._http_version;
	};
	HttpRequest& operator=(const HttpRequest& B) = delete;
	HttpRequest& operator=(HttpRequest&& B)
	{
		this->_method = B._method;
		this->_request_target = B._request_target;
		this->isValid = B.isValid;
		this->_headers = B._headers;
		this->_body = B._body;
		this->_http_version = B._http_version;
		return *this;
	};
	~HttpRequest(){};
	void SetMethod(std::string);
	void SetTarget(std::string);
	std::string GetStartLine() const override;
	std::string GetMethod() const
	{
		return _method;
	};
	std::string GetTarget() const
	{
		return _request_target;
	};
	bool isValid;

private:
	std::string _method;
	std::string _request_target;
};
class HttpResponse : public HttpMessage
{
public:
	HttpResponse()
	  : HttpMessage()
	  , _http_version("HTTP/1.1"){};
	HttpResponse(HttpResponse&& B)
	{
		this->_status_code = B._status_code;
		this->_reason_phrase = B._reason_phrase;
		this->_http_version = B._http_version;
		this->_headers = B._headers;
		this->_body = B._body;
		this->_http_version = B._http_version;
	};
	HttpResponse(std::promise<std::vector<unsigned char> >&& promise)
	  : _http_version("HTTP/1.1"){};
	HttpResponse(std::string);
	HttpResponse& operator=(HttpResponse&& B)
	{
		this->_status_code = B._status_code;
		this->_reason_phrase = B._reason_phrase;
		this->_http_version = B._http_version;
		this->_headers = B._headers;
		this->_body = B._body;
		this->_http_version = B._http_version;
		return *this;
	};
	~HttpResponse(){};
	void SetStatusCode(int);
	void SetReasonPhrase(std::string);
	inline int GetStatusCode() const
	{
		return _status_code;
	};
	std::string GetStartLine() const override;

private:
	int _status_code;
	std::string _reason_phrase;
	std::string _http_version;
};
#endif