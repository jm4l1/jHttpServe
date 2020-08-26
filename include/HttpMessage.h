#ifndef __HTTP_MSSAGE_H__
#define __HTTP_MSSAGE_H__

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <future>

#include "jjson.hpp"

#define SP char(32)
#define CR char(13)
#define LF char(10)

static std::vector<std::string> Methods({"GET" ,"HEAD" ,"POST" ,"PUT" ,"DELETE" ,"CONNECT" ,"OPTIONS" ,"TRACE"});

static std::unordered_map<int,std::string> ResponseCodes = {
    {200 ,"OK"},
    {400 , "Bad Request"},
    {401 , "Unauthorized"},
    {403 , "Forbidden"},
    {404 , "Not Found"},
    {500 , "Internal Server Error"},
    {501 , "Not Implemented"},
    {502 , "Bad Gateway"},
    {503 , "Service Unavailable"},
    {504 , "Gateway Timeout"},
    {505 , "HTTP Version Not Supported"}
};
class HttpMessage{
    public:
        HttpMessage() = default;
        virtual ~HttpMessage(){};
        std::string ToString() const;
        void SetHeader(std::string,std::string);
        void SetBody(const std::string&);
        void SetBody(const jjson::value&);
        std::optional<std::string> GetHeader(std::string) const;
        std::string GetBody() const;
        void SetVersion(std::string);
        virtual std::string GetStartLine() const{ return "";};
    protected:
        std::unordered_map<std::string,std::string> _headers;
        std::string _body;
        std::string _http_version = "HTTP/1.1";
};
class HttpRequest : public HttpMessage{
    public:
        HttpRequest():HttpMessage(){};
        HttpRequest(std::string);
        HttpRequest(const HttpRequest&B){
            this->_method = B._method;
            this->_request_target = B._request_target;
            this->isValid = B.isValid;
        };
        HttpRequest(HttpRequest&& B){
            this->_method = B._method;
            this->_request_target = B._request_target;
            this->isValid = B.isValid;
        };
        HttpRequest& operator=(const HttpRequest& B)=delete;
        HttpRequest& operator=(HttpRequest&& B){
            this->_method = B._method;
            this->_request_target = B._request_target;
            this->isValid = B.isValid;
            return *this;
        };
        ~HttpRequest(){};
        void SetMethod(std::string);
        void SetTarget(std::string);
        std::string GetStartLine() const override;
        std::string GetMethod(){ return _method;};
        std::string GetTarget(){ return _request_target;};
        bool isValid;
    private:
        std::string _method;
        std::string _request_target;
};
class HttpResponse : public HttpMessage{
    public:
        HttpResponse():HttpMessage(){};
        HttpResponse(HttpResponse&& B){
            this->_status_code = B._status_code;
            this->_reason_phrase = B._reason_phrase;
            this->_http_version = B._http_version;
            this->response_promise = std::move(response_promise);
        };
        HttpResponse& operator=(HttpResponse&& B){
            this->_status_code = B._status_code;
            this->_reason_phrase = B._reason_phrase;
            this->_http_version = B._http_version;
            this->response_promise = std::move(response_promise);
            return *this;
        };
        HttpResponse(std::promise<std::string> &&promise):response_promise(std::move(promise)),_http_version("HTTP/1.1"){};
        HttpResponse(std::string);
        ~HttpResponse(){};
        void SetStatusCode(int);
        void SetReasonPhrase(std::string);
        int GetStatusCode(){ return _status_code;};
        void Send();
    private:
        int _status_code;
        std::string _reason_phrase;
        std::string _http_version;
        std::string GetStartLine() const override;
        std::promise<std::string> response_promise;
};
#endif