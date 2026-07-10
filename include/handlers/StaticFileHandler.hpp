#ifndef STATICFILEHANDLER_HPP
#define STATICFILEHANDLER_HPP

#include "Config.hpp"
#include "Request.hpp"
#include "Response.hpp"

#include <string>

struct AutoindexEntry
{
    std::string name;
    bool isDirectory;
};

class StaticFileHandler
{
public:
    static Response handle(const Request &request, const RouteConfig &route, const ServerConfig &server);

private:
    StaticFileHandler();
    StaticFileHandler(const StaticFileHandler &other);
    StaticFileHandler &operator=(const StaticFileHandler &other);
    ~StaticFileHandler();
};

#endif