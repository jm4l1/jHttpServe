#ifndef _HTTPSERVER_H_
#define _HTTPSERVER_H_

#include<mutex>
#include <string>
#include <deque>
#include <chrono>
#include <iomanip>

#include "HttpMessage.h"
#include "jjson.hpp"
#include "SocketServer.h"
#include "RouteMap.h"

class MessageQueue{
    public:
        MessageQueue(){};
        std::pair<HttpRequest,HttpResponse> receive()
        {
            std::unique_lock<std::mutex> uLock(_mutex);
            _message_receive_con.wait(uLock,[this]{ return !_requests.empty();});
            auto request = std::move(_requests.front());
            _requests.pop_front();

            return request;
        };
        void Send(std::pair<HttpRequest,HttpResponse> &&req_res)
        {
            std::lock_guard<std::mutex> Lock(_mutex);
            _requests.push_back(std::move(req_res));
            _message_receive_con.notify_one();
        };
    private:
        std::mutex _mutex;
        std::condition_variable _message_receive_con;
        std::deque<std::pair<HttpRequest,HttpResponse>> _requests;
};
class HttpServer{
    public:
        HttpServer(){};
        ~HttpServer(){};
        void Init(std::string);
        void Get(std::string , std::function<void(HttpRequest&& , HttpResponse&&)>);
        void Post(std::string , std::function<void(HttpRequest&& , HttpResponse&&)>);
    private:
        jjson::value _config;
        MessageQueue _request_queue;
        SocketServer _socket_server;
        RouteMap _route_map;
        std::mutex _logger_mutex;
        std::vector<std::string> _allowed_methods;
        void ParseConfigFile(std::string);
        void HandleApplicationLayer();
        void HandleApplicationLayerSync(HttpRequest&&, HttpResponse&&);
        void HandleUpload(HttpRequest&&, HttpResponse&&);
        void HandleGetUploads(HttpRequest&&, HttpResponse&&);
        void Log(const HttpRequest& , const HttpResponse&);
        std::function<void(std::vector<unsigned char> , std::promise<std::vector<unsigned char> >)> handle_parse_layer = [this](std::vector<unsigned char> message_buffer, std::promise<std::vector<unsigned char> > &&response_promise){
             HttpRequest request = HttpRequest(std::move(message_buffer));
             HttpResponse response = HttpResponse(std::move(response_promise));
                response.SetHeader("server",(std::string)_config["server_name"]);
                response.SetHeader("date" , GetDate());
                response.SetHeader("connection","close");
             if(!request.isValid){
                response.SetStatusCode(400);
                Log(request,response);
                response.Send();
                return;
             }
            //  auto req_res = std::make_pair(std::move(request),std::move(response));
            // _request_queue.Send(std::move(req_res));
            // return;
            HandleApplicationLayerSync(std::move(request),std::move(response));
        };

        void ValidateMethod(std::string method ,const HttpRequest& request, HttpResponse& response)
        {
            std::stringstream body_stream;
            auto method_itr = std::find(_allowed_methods.begin(),_allowed_methods.end(),method);
            if(method_itr ==_allowed_methods.end())
            {
                response.SetStatusCode(405);
                std::stringstream allowed_stream;
                std::ostream_iterator<std::string> outputString(allowed_stream , ",");
                std::copy(_allowed_methods.begin(),_allowed_methods.end(),outputString);
                response.SetHeader("allow",allowed_stream.str());
                body_stream << "<body><div><H1>405 Method Not Allowed</H1>" << allowed_stream.str() << "</div></body>";
                response.SetBody(body_stream.str());
                Log(request,response);
                response.Send();
                return;
            }
        }
        static std::string GetDate()
        {
                 auto now = std::chrono::system_clock::now();
                 auto date = std::chrono::system_clock::to_time_t(now);
                 std::stringstream date_stream;
                 date_stream << std::put_time(std::gmtime(&date), "%a, %d %b %Y %OH:%M:%S GMT") ;
                 return date_stream.str();
        };
        static std::string GetshortDate()
        {
                 auto now = std::chrono::system_clock::now();
                 auto date = std::chrono::system_clock::to_time_t(now);
                 std::stringstream date_stream;
                 date_stream << std::put_time(std::localtime(&date), "%Y%m%d_%OH:%M:%S") ;
                 return date_stream.str();
        };
};

#endif