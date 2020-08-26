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
    private:
        jjson::value _config;
        MessageQueue _request_queue;
        SocketServer _socket_server;
        void ParseConfigFile(std::string);
        void HandleApplicationLayer();
        void HandleApplicationLayerSync(HttpRequest&& , HttpResponse&&);
        std::function<void(std::string , std::promise<std::string>)> handle_parse_layer = [this](std::string message_buffer, std::promise<std::string> &&response_promise){
             HttpRequest request = HttpRequest(message_buffer);
             HttpResponse response = HttpResponse(std::move(response_promise));
             if(!request.isValid){
                response.SetHeader("server",(std::string)_config["server_name"]);
                response.SetStatusCode(400);
                response.SetHeader("date" , GetDate());
                response.Send();
                return;
             }
            //  auto req_res = std::make_pair(std::move(request),std::move(response));
            // _request_queue.Send(std::move(req_res));
            // return;
            HandleApplicationLayerSync(std::move(request),std::move(response));
        };
        static std::string GetDate(){
                 auto now = std::chrono::system_clock::now();
                 auto date = std::chrono::system_clock::to_time_t(now);
                 std::stringstream date_stream;
                 date_stream << std::put_time(std::localtime(&date), "%F %T") ;
                 return date_stream.str();
        };
};

#endif