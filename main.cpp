#include "ArgsParser.h"
#include "HttpMessage.h"
#include "HttpServer.h"
#include "RouteMap.h"
#include "SocketServer.h"
#include "jjson.hpp"

#include <iostream>

int main(int argc, char* argv[])
{
	ArgsParser parser(argv[0]);
	parser.AddOption('f', "config_file", "location of configuration file in .json format", true);
	parser.AddOption('p', "port", "Tcp port to accept server connections", false);
	parser.AddOption('t', "timeout", "Time out of server to close connection", false);
	parser.Parse(argc, argv);

	auto argument_map = parser.GetArgs();
	auto file_name = argument_map['f'];
	auto port = 0;
	if (argument_map.find('p') != argument_map.end())
	{
		port = std::stoi(argument_map['p']);
	}
	auto timeout = 0;
	if (argument_map.find('t') != argument_map.end())
	{
		timeout = std::stoi(argument_map['t']);
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