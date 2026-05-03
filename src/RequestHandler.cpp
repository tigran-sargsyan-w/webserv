#include "RequestHandler.hpp"
#include "CgiHandler.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <string>
#include <utils.hpp>
#include "Config.hpp"
#include <vector>
#include <map>

RequestHandler::RequestHandler() {}

RequestHandler::~RequestHandler() {}

Response RequestHandler::handleStatic(const Request &request)
{
    Response res;

    std::string fullPath = "www" + request.getPath();
    if (fullPath == "www/")
        fullPath = "www/index.html";
    std::ifstream file(fullPath.c_str());
    if (!file)
    {
        res.setStatusCode(404);
        res.setBodyFromFile("www/error.html");
        std::cerr << "Resourse not found!" << std::endl; // TODO: return 404
                                                         // response
    }
    if (file)
    {
        res.setStatusCode(200);
        res.setBodyFromFile(fullPath);
    }
    return res;
}

static Response buildCgiResponse(const std::string &cgiOutput)
{
    Response response;

    std::string separator = "\r\n\r\n";
    size_t bodyStart = cgiOutput.find(separator);

    if (bodyStart == std::string::npos)
    {
        separator = "\n\n";
        bodyStart = cgiOutput.find(separator);
    }

    if (bodyStart != std::string::npos)
    {
        std::string cgiHeaders = cgiOutput.substr(0, bodyStart);
        std::string cgiBody = cgiOutput.substr(bodyStart + separator.length());

        response.setStatusCode(200);
        response.setBody(cgiBody);

        if (cgiHeaders.find("Content-Type:") != std::string::npos)
            response.addHeader("Content-Type", "text/html");
        else
            response.addHeader("Content-Type", "text/plain");
    }
    else
    {
        response.setStatusCode(200);
        response.setBody(cgiOutput);
        response.addHeader("Content-Type", "text/plain");
    }

    response.addHeader("Content-Length", intToString(response.getBody().length()));
    response.addHeader("Connection", "close");

    return (response);
}

static std::string getQueryString(const std::string &path)
{
    size_t questionMark = path.find('?');

    if (questionMark == std::string::npos)
        return ("");
    return (path.substr(questionMark + 1));
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

static std::string buildCgiScriptPath(const Request &request, const RouteConfig &route)
{
    std::string pathInsideRoute = getPathInsideRoute(request.getPath(), route);

    return (joinPaths(route.root, pathInsideRoute));
}

static std::string getFileExtension(const std::string &path)
{
    std::string cleanPath = getPathWithoutQuery(path);
    size_t dot = cleanPath.rfind('.');

    if (dot == std::string::npos)
        return ("");
    return (cleanPath.substr(dot));
}

static std::string getCgiExecutable(const std::string &path, const std::vector<CgiConfig> &cgiConfigs)
{
    std::string extension = getFileExtension(path);

    for (std::vector<CgiConfig>::const_iterator it = cgiConfigs.begin();
         it != cgiConfigs.end();
         ++it)
    {
        if (it->extension == extension)
            return (it->executable);
    }

    return ("");
}

static bool isCgiRequest(const std::string &path, const RouteConfig &route)
{
    return (!getCgiExecutable(path, route.cgi).empty());
}

static bool routeHasCgiConfig(const RouteConfig &route)
{
    return (!route.cgi.empty());
}

static std::string trimHeaderValue(const std::string &value)
{
    size_t start;
    size_t end;

    start = 0;
    while (start < value.length() && (value[start] == ' ' || value[start] == '\t'))
        start++;
    end = value.length();
    while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '\r'))
        end--;
    return (value.substr(start, end - start));
}

static std::string getHeaderValue(const Request &request, const std::string &name)
{
    std::map<std::string, std::string>::const_iterator it;

    it = request.getHeaders().find(name);
    if (it != request.getHeaders().end())
        return (trimHeaderValue(it->second));
    return ("");
}

static std::string getContentLength(const Request &request)
{
    std::string contentLength;

    contentLength = getHeaderValue(request, "Content-Length");
    if (!contentLength.empty())
        return (contentLength);
    if (!request.getBody().empty())
        return (intToString(request.getBody().length()));
    return ("");
}

static void addStandardCgiVariables(CgiContext &context, const Request &request, const ServerConfig &server)
{
    context.standard.values["AUTH_TYPE"] = "";
    context.standard.values["CONTENT_LENGTH"] = getContentLength(request);
    context.standard.values["CONTENT_TYPE"] = getHeaderValue(request, "Content-Type");
    context.standard.values["GATEWAY_INTERFACE"] = "CGI/1.1";
    context.standard.values["PATH_INFO"] = "";
    context.standard.values["PATH_TRANSLATED"] = "";
    context.standard.values["QUERY_STRING"] = getQueryString(request.getPath());
    context.standard.values["REMOTE_ADDR"] = "";
    context.standard.values["REMOTE_HOST"] = "";
    context.standard.values["REMOTE_IDENT"] = "";
    context.standard.values["REMOTE_USER"] = "";
    context.standard.values["REQUEST_METHOD"] = request.getMethod();
    context.standard.values["SCRIPT_NAME"] = getPathWithoutQuery(request.getPath());
    context.standard.values["SERVER_NAME"] = server.serverName;
    context.standard.values["SERVER_PORT"] = intToString(server.listen.port);
    context.standard.values["SERVER_PROTOCOL"] = request.getVersion();
    context.standard.values["SERVER_SOFTWARE"] = "webserv/1.0";
}

static CgiContext buildCgiContext(const Request &request, const RouteConfig &route, const ServerConfig &server, const std::string &executable)
{
    CgiContext context;

    context.executable = executable;
    context.scriptPath = buildCgiScriptPath(request, route);
    context.requestBody = request.getBody();
    addStandardCgiVariables(context, request, server);
    return (context);
}

Response RequestHandler::handleRequest(const Request &request, const RouteConfig &route, const ServerConfig &server)
{
    // Generate a response
    Response response;

    if (isCgiRequest(request.getPath(), route))
    {
        std::string executable = getCgiExecutable(request.getPath(), route.cgi);
        CgiContext context = buildCgiContext(request, route, server, executable);
        std::cout << "CGI script path: " << context.scriptPath << std::endl;
        std::string cgiOutput = CgiHandler::runCgi(context);
        return (buildCgiResponse(cgiOutput));
    }

    if (routeHasCgiConfig(route))
    {
        Response response;
        response.setStatusCode(403);
        response.setBody("<html><body><h1>403 Forbidden</h1></body></html>");
        response.addHeader("Content-Type", "text/html");
        response.addHeader("Content-Length", intToString(response.getBody().length()));
        response.addHeader("Connection", "close");
        return (response);
    }

    // if (!request.getIsCgi())
    //   CgiHandler::runCgi();
    if (request.getMethod() == "GET")
    {
        response = RequestHandler::handleStatic(request);
    }
    else
    {
        response.setStatusCode(405);
        response.setBody(
            "<html><body><h1>405 Method Not Allowed</h1></body></html>");
    }
    std::ostringstream oss;
    oss << response.getBody().length();
    response.addHeader("Content-Type", "text/html");
    response.addHeader("Connection", "close");
    response.addHeader("Content-Length", oss.str());
    return response;
}
