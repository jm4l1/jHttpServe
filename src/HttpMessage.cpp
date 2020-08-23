#include "HttpMessage.h"
#include <sstream>
#include <iostream>
#include <string>

std::string HttpMessage::ToString() const{
    std::stringstream http_stream;
    http_stream << GetStartLine() << CR << LF;
    for(auto header : _headers){
        http_stream << header.first << ": " << header.second << CR << LF;
    }
    http_stream << CR << LF ;
    http_stream << _body;
    return http_stream.str();
};
void HttpMessage::SetHeader(std::string header_name ,std::string header_value){
    _headers[header_name] = header_value;
};
void HttpMessage::SetBody(std::string body){
    _body = body;
};
std::optional<std::string >HttpMessage::GetHeader(std::string header_name) const{
    auto kv_pair = _headers.find(header_name);
    if(kv_pair == _headers.end()){
        return {};
    }
    return kv_pair->second;
};
std::string HttpMessage::GetBody() const{
    return _body;
};
void HttpMessage::SetVersion(std::string version){
    _http_version = version;
};


HttpRequest::HttpRequest(std::string request_string){
    auto request_stream = std::istringstream(request_string);
    std::string stream_line;
    
    //status line
    std::getline(request_stream, stream_line);
    std::stringstream status_line_stream(stream_line);
    status_line_stream >> _method >> _request_target >> _http_version;

    //headers
    while(std::getline(request_stream, stream_line)){
       std::istringstream header_line_stream(stream_line);
       std::string header_name, header_value;
       std::getline(header_line_stream,header_name,':');
       header_line_stream >> header_value;
       SetHeader(header_name,header_value);
    }
    //body
    std::string body_string;
    while(std::getline(request_stream, stream_line)){
       body_string.append(stream_line).append("\n");
    }
    SetBody(body_string);
};
void HttpRequest::SetMethod(std::string method){
    _method = method;
}
void HttpRequest::SetTarget(std::string target){
    _request_target = target;
};
std::string HttpRequest::GetStartLine() const{
    std::stringstream start_line_stream;
    start_line_stream << _method << " " << _request_target << " " << _http_version;
    return start_line_stream.str();
};

HttpResponse::HttpResponse(std::string response_string){
     auto request_stream = std::stringstream(response_string);
    std::string stream_line;

    //status line
    std::getline(request_stream, stream_line);
    std::istringstream status_line_stream(stream_line);
    status_line_stream >> _http_version >> _status_code >> _reason_phrase ;
    std::getline(request_stream, stream_line);

    //headers
    while(std::getline(request_stream, stream_line) && (stream_line != "") ){
       std::istringstream header_line_stream(stream_line);
       std::string header_name, header_value;
       std::getline(header_line_stream,header_name,':');
       header_line_stream >> header_value;
       SetHeader(header_name,header_value);
    }
    //body
    std::string body_string;
    while(std::getline(request_stream, stream_line)){
       body_string.append(stream_line).append("\n");
    }
    SetBody(body_string);
};
void HttpResponse::SetStatusCode(int status_code){
    _status_code = status_code;
};
void HttpResponse::SetReasonPhrase(std::string reason_phrase){
    _reason_phrase = reason_phrase;
};
std::string HttpResponse::GetStartLine() const{
    std::stringstream start_line_stream;
    start_line_stream << _http_version << " " << _status_code << " " << _reason_phrase;
    return start_line_stream.str();

};

