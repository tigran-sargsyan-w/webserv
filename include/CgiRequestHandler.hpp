#ifndef CGIREQUESTHANDLER_HPP
#define CGIREQUESTHANDLER_HPP

#include "Config.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include "CgiHandler.hpp"

#include <string>

class CgiRequestHandler
{
public:
    static bool isCgiRequest(const Request &request, const RouteConfig &route);
    static Response handle(const Request &request, const RouteConfig &route, const ServerConfig &server, const std::string &remoteAddr);
    static CgiContext buildContext(const Request &request, const RouteConfig &route, const ServerConfig &server, const std::string &remoteAddr);
    static Response buildResponse(const std::string &cgiOutput);

private:
    CgiRequestHandler();
    CgiRequestHandler(const CgiRequestHandler &other);
    CgiRequestHandler &operator=(const CgiRequestHandler &other);
    ~CgiRequestHandler();
};

#endif