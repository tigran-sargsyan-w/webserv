#ifndef REDIRECTHANDLER_HPP
#define REDIRECTHANDLER_HPP

#include "Config.hpp"
#include "Response.hpp"

class RedirectHandler
{
public:
    static Response handle(const RouteConfig &route, const ServerConfig &server);

private:
    RedirectHandler();
    RedirectHandler(const RedirectHandler &other);
    RedirectHandler &operator=(const RedirectHandler &other);
    ~RedirectHandler();
};

#endif