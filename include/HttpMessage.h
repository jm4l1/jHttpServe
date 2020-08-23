#ifndef __HTTP_MSSAGE_H__
#define __HTTP_MSSAGE_H__

#include <string>
#include <unordered_map>
#include <optional>

#define SP char(32)
#define CR char(13)
#define LF char(10)

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
        std::string _http_version;
};
class HttpRequest : public HttpMessage{
    public:
        HttpRequest():HttpMessage(){};
        HttpRequest(std::string);
        ~HttpRequest(){};
        void SetMethod(std::string);
        void SetTarget(std::string);
        std::string GetStartLine() const override;
    private:
        std::string _method;
        std::string _request_target;
};
class HttpResponse : public HttpMessage{
    public:
        HttpResponse():HttpMessage(){};
        HttpResponse(std::string);
        ~HttpResponse(){};
        void SetStatusCode(int);
        void SetReasonPhrase(std::string);
        int GetStatusCode(){ return _status_code;};
    private:
        int _status_code;
        std::string _reason_phrase;
        std::string _http_version;
        std::string GetStartLine() const override;
};
#endif