#include <iostream>
#include <fstream>

#include "HttpServer.h"


namespace fs = std::filesystem;

void HttpServer::Get(std::string target, std::function<void(HttpRequest&& , HttpResponse&&)> get_handler)
{
    _route_map.RegisterRoute("GET"+target , get_handler);
}
void HttpServer::Post(std::string target , std::function<void(HttpRequest&& , HttpResponse&&)> post_handler)
{
    _route_map.RegisterRoute("POST"+target , post_handler);
}

void HttpServer::Init(std::string config_file_name){
    ParseConfigFile(config_file_name);
    int port = (int)_config["port"];
    _socket_server.SetPort(port);
    _socket_server.CreateSocket();
    _socket_server.Listen(handle_parse_layer);
    // std::thread socket_thread([this](){
    //     _socket_server.Listen(handle_parse_layer);
    // });
    // auto  app_logic_thread = std::thread(&HttpServer::HandleApplicationLayer , this);
    // app_logic_thread.detach();
    // socket_thread.join();

};

void HttpServer::Log(const HttpRequest &request, const HttpResponse &response){
    // std::lock_guard<std::mutex> lock(_logger_mutex);
    std::cout << response.GetHeader("date").value_or("") << " " << request.GetStartLine() <<  " " << response.GetStatusCode() << " -\n"; 
};
void HttpServer::HandleApplicationLayerSync(HttpRequest&& request, HttpResponse&& response)
{
    std::stringstream body_stream;

    //check method allowed

    //check for request 
    auto method = request.GetMethod();
    auto target = request.GetTarget();

    //check route map for requested resource
    auto request_handler = _route_map.GetRouteHandler(method+target).value_or(nullptr);
    if(request_handler){
        request_handler(std::move(request) , std::move(response));
        return;
    }
    target = target == "/" ?  "/index.html" : target;
    //fall back to web dir
    auto target_location = (std::string)_config["web_dir"] + "/" + target;
    if(!fs::exists(fs::path(target_location)))
    {
        response.SetStatusCode(404);
        response.SetHeader("content-type","text/html");
        body_stream << "<body><div><H1>404 Not Found</H1>"  << target << " not found. "<< _route_map.GetRoutes() << "</div></body>";
        response.SetBody(body_stream.str());
        Log(request,response);
        response.Send();
        return;
    }
    std::ifstream target_file(target_location);
    if(!target_file.is_open())
    {
        response.SetStatusCode(500);
        response.SetHeader("content-type","text/html");
        body_stream << "<body><H1>500 Internal Server Error</H1><div>.</div></body>";
        response.SetBody(body_stream.str());
        Log(request,response);
        response.Send();
        return;
    }

    std::string target_string;
    std::string line;
    while (std::getline(target_file, line))
    {
        target_string.append(line);
    }
    response.SetStatusCode(200);
    response.SetHeader("content-type","text/html");
    response.SetBody(target_string);
    Log(request,response);
    response.Send();
    return;
};
void HttpServer::HandleApplicationLayer(){
    std::cout << "Application Layer Started\n";
    while(1)
    {
        auto req_res = _request_queue.receive();

        std::cout << "request recieved\n";
        auto request = std::move(req_res.first);
        auto response = std::move(req_res.second);
        
        std::stringstream body_stream;
        body_stream << "<H1>Hello there</H1>" << request.ToString();
        auto body_string = body_stream.str();

        response.SetStatusCode(200);
        response.SetHeader("content-type","text/html");
        response.SetBody(body_string);
        Log(request,response);
        response.Send();
        return;
    }
};
void HttpServer::ParseConfigFile(std::string file_name){
    if(!fs::exists(fs::path(file_name)))
    {
        std::cout << "Config file could not be found!!\n";
        exit(EXIT_FAILURE);
    }

    std::ifstream config_file(file_name);
    if(!config_file.is_open())
    {
        std::cout << "Error Opening Config file!\n";
        exit(EXIT_FAILURE);
    }
    
    std::string config_string;
    std::string line;
    while (std::getline(config_file, line)) {
        config_string.append(line);
    }
    
    _config = jjson::value::parse_from_string(config_string);
    if(!_config.is_valid()){
        std::cout << "Error Parsing Config file. Please verify valid JSON\n";
        exit(EXIT_FAILURE);
    }
    if(!_config.HasKey("port")){
        std::cout << "port is required in config file\n";
        exit(EXIT_FAILURE);
    }
    if(!_config.HasKey("web_dir")){
        std::cout << "web_dir location is required in config file\n";
        exit(EXIT_FAILURE);
    }
    std::string web_dir = (std::string)_config["web_dir"];
    if(!fs::exists(fs::path(web_dir)))
    {
        std::cout << "web dir could not be found!\n";
        exit(EXIT_FAILURE);
    }
    std::cout << "config file loaded\n";
}