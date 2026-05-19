#ifndef REDIRECTHANDLER_HPP
#define REDIRECTHANDLER_HPP

#include "Config.hpp"
#include "Response.hpp"

class RedirectHandler
{
public:
    static Response handle(const RouteConfig &route);

private:
    RedirectHandler();
    RedirectHandler(const RedirectHandler &other);
    RedirectHandler &operator=(const RedirectHandler &other);
    ~RedirectHandler();
};

#endif