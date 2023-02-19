#include "HttpMessage.h"
#include "HttpServer.h"
#include "RouteMap.h"
#include "SocketServer.h"
#include "jjson.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string_view>

namespace fs = std::filesystem;

void print_usage(std::string_view prog_name)
{
	std::cout << "Usage: " << prog_name << " [-f config_file] [-h] [-p port] [-t timeout]"
			  << "\n";
	std::cout << "\t-f config_file : location of configuration file in .json format\n";
	std::cout << "\t-p port : Tcp port to accept server connections\n";
	std::cout << "\t-t timeout : Time out of server to close connection \n";
	std::cout << "\t-h : show help menu\n";
}

std::unordered_map<std::string, std::string>
	parse_argument(int argc, char* argv[], std::string_view allowed_flags, std::vector<std::string> required_flags)
{
	if (argc < 2)
	{
		std::cout << "Insufficient arguments provided!!\n\n";
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	std::unordered_map<std::string, std::string> argument_map;
	int count = 1;
	while (count < (argc - 1))
	{
		std::string arg = argv[count];
		std::regex flag_regex("^-(.)");
		std::smatch flag_match;
		if (!std::regex_match(arg, flag_match, flag_regex))
		{
			print_usage(argv[0]);
			exit(EXIT_FAILURE);
		};
		if (std::find(allowed_flags.begin(), allowed_flags.end(), flag_match[1]) == allowed_flags.end())
		{
			std::cout << "Error : Invalid flag " << arg << "\n";
			print_usage(argv[0]);
			exit(EXIT_FAILURE);
		}
		if (flag_match[1] == "h")
		{
			print_usage(argv[0]);
			exit(EXIT_SUCCESS);
		}
		if (count + 1 > argc)
		{
			std::cout << "Error : Insufficient arguments provided!!\n\n";
			print_usage(argv[0]);
			exit(EXIT_FAILURE);
		}
		argument_map.insert({ flag_match[1], argv[count + 1] });
		count += 2;
	}
	for (auto flag : required_flags)
	{
		if (argument_map.find(flag) == argument_map.end())
		{
			std::cout << "Error : Flag -" << flag << " is required\n\n";
			print_usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}
	return argument_map;
}

int main(int argc, char* argv[])
{
	auto argument_map = parse_argument(argc, argv, "hfpt", { "f" });
	auto file_name = argument_map["f"];
	auto port = 0;
	if (argument_map.find("p") != argument_map.end())
	{
		port = std::stoi(argument_map["p"]);
	}
	auto timeout = 0;
	if (argument_map.find("t") != argument_map.end())
	{
		timeout = std::stoi(argument_map["t"]);
	}
	auto server = HttpServer();
	auto get_api = [](HttpRequest&& request, HttpResponse&& response)
	{
		auto api_object = jjson::Object();
		api_object["running"] = true;
		response.SetStatusCode(200);
		response.SetHeader("content-type", "application/json");
		response.SetBody(api_object);
		response.Send();
	};
	server.Get("/api", get_api);
	server.Init(file_name);
}