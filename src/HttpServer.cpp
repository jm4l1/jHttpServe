#include <iostream>
#include <fstream>

#include "HttpServer.h"


namespace fs = std::filesystem;

void HttpServer::Init(std::string config_file_name){
    ParseConfigFile(config_file_name);
    int port = (int)_config["port"];
    _socket_server.SetPort(port);
    _socket_server.CreateSocket();
    _socket_server.Listen(handle_parse_layer);
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