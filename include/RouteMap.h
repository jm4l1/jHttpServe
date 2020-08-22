#ifndef _ROUTEMAP_H_
#define _ROUTEMAP_H_

#include <unordered_map>
#include <functional>
#include <string>
#include <optional>

class RouteMap{
    public:
        void RegisterRoute(std::string,std::function<void()>);
        void UnregisterRoute(std::string);
        std::optional<std::function<void()>> GetRouteHandler(std::string);
        bool HasRoute(std::string);
        std::string GetRoutes();
        void ShowRoutes();
    private:
        std::unordered_map<std::string, std::function<void()>> routes;
};

#endif