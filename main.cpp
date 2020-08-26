#include "SocketServer.h"
#include "RouteMap.h"
#include "HttpMessage.h"
#include "jjson.hpp"
#include "HttpServer.h"
#include <iostream>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

int main(){
    auto file_name = "/Users/mali/Developer/mali/cppfiles/CppND-Capstone-http-server/server.json";
    auto server = HttpServer();
    server.Init(file_name);
    // if(!fs::exists(fs::path(file_name)))
    // {
    //     std::cout << "Config file could not be found!!\n";
    //     exit(EXIT_FAILURE);
    // }

    // std::ifstream config_file(file_name);
    // if(!config_file.is_open())
    // {
    //     std::cout << "Error Opening Config file!\n";
    //     exit(EXIT_FAILURE);
    // }
    
    // std::string config_string;
    // std::string line;
    // while (std::getline(config_file, line)) {
    //     config_string.append(line);
    // }
    // auto config_object = jjson::value::parse_from_string(config_string);
    // if(!config_object.is_valid()){
    //     std::cout << "Error Parsing Config file. Please verify valid JSON\n";
    //     exit(EXIT_FAILURE);
    // }
    // if(!config_object.HasKey("port")){
    //     std::cout << "port is required in config file\n";
    //     exit(EXIT_FAILURE);
    // }
    // if(!config_object.HasKey("web_dir")){
    //     std::cout << "web_dir location is required in config file\n";
    //     exit(EXIT_FAILURE);
    // }
    // std::string web_dir = (std::string)config_object["web_dir"];
    // if(!fs::exists(fs::path(web_dir)))
    // {
    //     std::cout << "web dir could not be found!\n";
    //     exit(EXIT_FAILURE);
    // }
    // std::cout << "config file loaded\n";
    // int port = int(config_object["port"]);

    // auto s = SocketServer(port);

    // auto e = HttpResponse(R"(HTTP/1.1 301 Moved Permanently
    //                 Server: nginx
    //                 Date: Sat, 22 Aug 2020 17:24:40 GMT
    //                 Content-Type: text/html
    //                 Content-Length: 178
    //                 Connection: keep-alive
    //                 Location: https://www.goal.com/
    //                 X-Server-Id: fadda5cf6fb9b00221bdcfc7b43ef36d038a9009

    //                 <html>
    //                 <head><title>301 Moved Permanently</title></head>
    //                 <body bgcolor="white">
    //                 <center><h1>301 Moved Permanently</h1></center>
    //                 <hr><center>nginx</center>
    //                 </body>
    //                 </html>)");
              
    // s.Listen([](std::string buffer, std::promise<std::string> &&response_promise)
    //     {
    //         std::cout <<  "received message \n---" << buffer <<"---\n";
    //         response_promise.set_value("Your message was resceived\n");  
    //     });         
//     s.Listen([](std::string buffer, std::promise<std::string> &&response_promise)
//         {
//             //Parsing Layer handler
//             auto request = HttpRequest(buffer);
//             auto response = HttpResponse(std::move(response_promise));
//             response.SetHeader("Server","CPPND HTTP  Server");
//             if(!request.isValid){
//                 response.SetStatusCode(400);
//                 response.Send();
//                 return;
//             }
//             // auto response = HttpResponse(R"(HTTP/1.1 301 Moved Permanently
//             //         Server: nginx
//             //         Date: Sat, 22 Aug 2020 17:24:40 GMT
//             //         Content-Type: text/html
//             //         Content-Length: 178
//             //         Connection: keep-alive
//             //         Location: https://www.goal.com/
//             //         X-Server-Id: fadda5cf6fb9b00221bdcfc7b43ef36d038a9009

//             //         <html>
//             //         <head><title>301 Moved Permanently</title></head>
//             //         <body bgcolor="white">
//             //         <center><h1>301 Moved Permanently</h1></center>
//             //         <hr><center>nginx</center>
//             //         </body>
//             //         </html>)"
//             // );
//             response.SetBody(R"(<html>
// <head><title>301 Moved Permanently</title></head>
// <body bgcolor="white">
// <center><h1>301 Moved Permanently</h1></center>
// <hr><center>nginx</center>
// </body>
// </html>)");
//             response.Send();
//         });
    
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
    
//     auto response = HttpResponse(R"(HTTP/1.1 301 Moved Permanently
// Server: nginx
// Date: Sat, 22 Aug 2020 17:24:40 GMT
// Content-Type: text/html
// Content-Length: 178
// Connection: keep-alive
// Location: https://www.goal.com/
// X-Server-Id: fadda5cf6fb9b00221bdcfc7b43ef36d038a9009

// <html>
// <head><title>301 Moved Permanently</title></head>
// <body bgcolor="white">
// <center><h1>301 Moved Permanently</h1></center>
// <hr><center>nginx</center>
// </body>
// </html>)");
//     auto request = HttpRequest(R"(GET / HTTP/1.1
// Host: goal.com
// Connection: keep-alive
// Upgrade-Insecure-Requests: 1
// User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_5) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/84.0.4147.135 Safari/537.36
// Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9
// Accept-Encoding: gzip, deflate
// Accept-Language: en-GB,en-US;q=0.9,en;q=0.8)");
//     std::cout << response.GetStatusCode()<< "\n";
//     response.SetStatusCode(200);
//     std::cout << response.GetStatusCode()<< "\n";
//     std::cout << response.ToString() << "\n";
//     std::cout << request.ToString() << "\n";
//     config_file.close();
//     return 0;
}