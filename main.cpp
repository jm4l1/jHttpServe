#include "SocketServer.h"
#include "RouteMap.h"

#include <iostream>
int main(){
    // auto s = SocketServer(12345);
    // s.Listen([](std::string buffer, std::promise<std::string> &&response_promise)
    //     {
    //         std::cout <<  "received message \n---" << buffer <<"---\n";
    //         response_promise.set_value("Your message was resceived\n");  
    //     });
    
    auto rm = RouteMap();
    rm.RegisterRoute("GET /" ,[](){return;});
    rm.RegisterRoute("POST /" ,[](){return;});
    rm.RegisterRoute("GET /home" ,[](){return;});
    rm.RegisterRoute("DELETE /" ,[](){return;});
    std::cout <<rm.GetRoutes() << "\n";
    rm.UnregisterRoute("DELETE /"
    );
    std::cout <<rm.GetRoutes() << "\n";
    rm.UnregisterRoute("DELETE /"
    );
    std::cout << "the route map " << (rm.HasRoute("GET /") ? "has " : "does not have ") << " the route GET_/ \n"; 
    std::cout << "the route map " << (rm.HasRoute("GET /files") ? "has " : "does not have ") << " the route GET_/ \n"; 
    
    return 0;
}