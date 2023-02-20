#ifndef _ROUTEMAP_H_
#define _ROUTEMAP_H_

#include "HttpMessage.h"

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

class RouteMap
{
public:
	void RegisterRoute(std::string, std::function<HttpResponse(HttpRequest&&)>);
	void UnregisterRoute(std::string);
	std::optional<std::function<HttpResponse(HttpRequest&&)>> GetRouteHandler(std::string);
	bool HasRoute(std::string);
	std::string GetRoutes();
	void ShowRoutes();

private:
	std::unordered_map<std::string, std::function<HttpResponse(HttpRequest&&)>> routes;
};

#endif