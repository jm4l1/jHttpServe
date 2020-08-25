#ifndef __HTTP_MSSAGE_H__
#define __HTTP_MSSAGE_H__

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <future>

#define SP char(32)
#define CR char(13)
#define LF char(10)

static std::vector<std::string> Methods({"GET" ,"HEAD" ,"POST" ,"PUT" ,"DELETE" ,"CONNECT" ,"OPTIONS" ,"TRACE"});

static std::unordered_map<int,std::string> ResponseCodes = {
    {200 ,"OK"},
    {400 , "Bad Request"}
    
};
class HttpMessage{
    public:
        HttpMessage() = default;
        virtual ~HttpMessage(){};
        std::string ToString() const;
        void SetHeader(std::string,std::string);
        void SetBody(std::string);
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
        ~HttpRequest(){};
        void SetMethod(std::string);
        void SetTarget(std::string);
        std::string GetStartLine() const override;
        bool isValid;
    private:
        std::string _method;
        std::string _request_target;
};
class HttpResponse : public HttpMessage{
    public:
        HttpResponse():HttpMessage(){};
        HttpResponse(std::promise<std::string> &&promise):response_promise(std::move(promise)){}
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