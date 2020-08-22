#include "SocketServer.h"
#include <iostream>
// #include <future>

int main(){
    auto s = SocketServer(12345);
    s.Listen([](std::string buffer, std::promise<std::string> &&response_promise)
        {
            std::cout <<  "received message \n---" << buffer <<"---\n";
            response_promise.set_value("Your message was resceived\n");  
        });
    return 0;
}