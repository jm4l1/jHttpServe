#include "SocketServer.h"
#include "RouteMap.h"
#include "HttpMessage.h"

#include <iostream>
int main(){
    // auto s = SocketServer(12345);
    // s.Listen([](std::string buffer, std::promise<std::string> &&response_promise)
    //     {
    //         std::cout <<  "received message \n---" << buffer <<"---\n";
    //         response_promise.set_value("Your message was resceived\n");  
    //     });
    
    // auto rm = RouteMap();
    // rm.RegisterRoute("GET /" ,[](){return;});
    // rm.RegisterRoute("POST /" ,[](){return;});
    // rm.RegisterRoute("GET /home" ,[](){return;});
    // rm.RegisterRoute("DELETE /" ,[](){return;});
    // std::cout <<rm.GetRoutes() << "\n";
    // rm.UnregisterRoute("DELETE /"
    // );
    // std::cout <<rm.GetRoutes() << "\n";
    // rm.UnregisterRoute("DELETE /"
    // );
    // std::cout << "the route map " << (rm.HasRoute("GET /") ? "has " : "does not have ") << " the route GET_/ \n"; 
    // std::cout << "the route map " << (rm.HasRoute("GET /files") ? "has " : "does not have ") << " the route GET_/ \n"; 
    
    auto response = HttpResponse(R"(HTTP/1.1 301 Moved Permanently
Server: nginx
Date: Sat, 22 Aug 2020 17:24:40 GMT
Content-Type: text/html
Content-Length: 178
Connection: keep-alive
Location: https://www.goal.com/
X-Server-Id: fadda5cf6fb9b00221bdcfc7b43ef36d038a9009

<html>
<head><title>301 Moved Permanently</title></head>
<body bgcolor="white">
<center><h1>301 Moved Permanently</h1></center>
<hr><center>nginx</center>
</body>
</html>)");
    auto request = HttpRequest(R"(GET / HTTP/1.1
Host: goal.com
Connection: keep-alive
Upgrade-Insecure-Requests: 1
User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_5) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/84.0.4147.135 Safari/537.36
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9
Accept-Encoding: gzip, deflate
Accept-Language: en-GB,en-US;q=0.9,en;q=0.8)");
    std::cout << response.GetStatusCode()<< "\n";
    response.SetStatusCode(200);
    std::cout << response.GetStatusCode()<< "\n";
    std::cout << response.ToString() << "\n";
    std::cout << request.ToString() << "\n";
    return 0;
}