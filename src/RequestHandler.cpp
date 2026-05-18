#include "ErrorResponseBuilder.hpp"
#include "StaticFileHandler.hpp"
#include "CgiRequestHandler.hpp"
#include "RedirectHandler.hpp"
#include "RequestHandler.hpp"
#include <fstream>
#include <iostream>
#include <cstdio>
#include <sstream>
#include <string>

#include <utils.hpp>
#include <map>
#include <sys/stat.h>

static std::string getPathWithoutQuery(const std::string &path);
static std::string getPathInsideRoute(const std::string &requestPath, const RouteConfig &route);
static std::string joinPaths(const std::string &left, const std::string &right);

RequestHandler::RequestHandler() {}

RequestHandler::~RequestHandler() {}

Response RequestHandler::handleStatic(const Request &request, const RouteConfig &route)
{
    return (StaticFileHandler::handle(request, route));
}

static std::string getPathWithoutQuery(const std::string &path)
{
    size_t questionMark = path.find('?');

    if (questionMark == std::string::npos)
        return (path);
    return (path.substr(0, questionMark));
}

static std::string getPathInsideRoute(const std::string &requestPath, const RouteConfig &route)
{
    std::string cleanPath = getPathWithoutQuery(requestPath);

    if (route.path == "/")
        return (cleanPath);

    if (cleanPath.find(route.path) != 0)
        return (cleanPath);

    return (cleanPath.substr(route.path.length()));
}

static std::string joinPaths(const std::string &left, const std::string &right)
{
    if (left.empty())
        return (right);
    if (right.empty())
        return (left);

    if (left[left.length() - 1] == '/' && right[0] == '/')
        return (left + right.substr(1));
    if (left[left.length() - 1] != '/' && right[0] != '/')
        return (left + "/" + right);
    return (left + right);
}

Response RequestHandler::handlePost(const Request &request, const RouteConfig &route)
{
    Response res;

    // TODO: check that the body size isn't bigger than the route's client max body size

    // 1. Create the complete route
    std::string fullPath = joinPaths(route.root, getPathInsideRoute(request.getPath(), route));

    // 2. Check if it's a folder
    struct stat pathStat;
    if (stat(fullPath.c_str(), &pathStat) == 0 && S_ISDIR(pathStat.st_mode))
        return ErrorResponseBuilder::build(403, "Forbidden: Is a directory");

    // 3. Check the body
    if (request.getBody().empty())
        return ErrorResponseBuilder::build(400, "Bad request");

    // 4. Try to write the file
    std::ofstream file(fullPath.c_str(), std::ios::out | std::ios::binary);
    if (!file.is_open())
        return ErrorResponseBuilder::build(500, "Internal Server Error: Could not open file");

    file << request.getBody();
    file.close();

    // 5. Success response
    res.setStatusCode(201);
    res.addHeader("Content-Type", "text/html");
    res.setBody("<html><body><h1>201 Created: File uploaded</h1></body></html>");

    return res;
}

Response RequestHandler::handleHttpDelete(const Request &request, const RouteConfig &route)
{
    Response res;

    // 1. Create the complete route
    std::string fullPath = joinPaths(route.root, getPathInsideRoute(request.getPath(), route));

    // 2. Check if the ressource exists
    struct stat pathStat;
    if (stat(fullPath.c_str(), &pathStat) != 0)
        return ErrorResponseBuilder::build(404, "Not Found: Resource does not exist");

    // 4. Forbid the deletion of folders
    if (S_ISDIR(pathStat.st_mode))
        return ErrorResponseBuilder::build(403, "Forbidden: Cannot delete a directory");

    // 5. Try to delete
    if (std::remove(fullPath.c_str()) == 0)
    {
        Response res;
        res.setStatusCode(200);
        res.addHeader("Content-Type", "text/html");
        res.setBody("<html><body><h1>File deleted successfully</h1></body></html>");
        return res;
    }
    else
        return ErrorResponseBuilder::build(500, "Internal Server Error: Failed to delete the file");

    return res;
}

static bool hasHeader(const Response &response, const std::string &name)
{
    std::map<std::string, std::string> headers;

    headers = response.getHeaders();
    return (headers.find(name) != headers.end());
}

Response RequestHandler::handleRequest(const Request &request, const RouteConfig &route, const ServerConfig &server, const std::string &remoteAddr)
{
    // Generate a response
    Response response;

    // TODO: Move redirect after route method validation when method enforcement is implemented.
    if (route.hasReturn)
        return (RedirectHandler::handle(route));

    if (CgiRequestHandler::isCgiRequest(request, route))
        return (CgiRequestHandler::handle(request, route, server, remoteAddr));

    if (!route.cgi.empty())
        return (ErrorResponseBuilder::build(403, "Forbidden"));

    if (request.getMethod() == "GET")
    {
        response = RequestHandler::handleStatic(request, route);
    }
    else if (request.getMethod() == "POST")
    {
        response = RequestHandler::handlePost(request, route);
    }
    else if (request.getMethod() == "DELETE")
    {
        response = RequestHandler::handleHttpDelete(request, route);
    }
    else
    {
        std::cout << "Test else" << std::endl;
        return ErrorResponseBuilder::build(405, "Method Not Allowed");
    }

    std::ostringstream oss;
    oss << response.getBody().length();

    if (!hasHeader(response, "Content-Type"))
        response.addHeader("Content-Type", "text/html");

    response.addHeader("Connection", "close");
    response.addHeader("Content-Length", oss.str());

    return response;
}
