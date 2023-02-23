#include "RouteMap.h"

#include <sstream>

void RouteMap::RegisterRoute(std::string route_key, std::function<HttpResponse(HttpRequest&&)> route_handler)
{
	routes.emplace(std::make_pair(route_key, route_handler));
}
bool RouteMap::HasRoute(std::string route)
{
	auto route_handler = routes.find(route);
	return route_handler != routes.end();
}
void RouteMap::UnregisterRoute(std::string route_key)
{
	routes.erase(route_key);
}
std::optional<std::function<HttpResponse(HttpRequest&&)>> RouteMap::GetRouteHandler(std::string route)
{
	auto route_handler = routes.find(route);
	if (route_handler == routes.end())
	{
		return {};
	}
	return route_handler->second;
}
std::string RouteMap::GetRoutes()
{
	std::stringstream route_stream;
	for (auto route_itr : routes)
	{
		route_stream << route_itr.first << "\n";
	}
	return route_stream.str();
}
