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

#include <cctype>

#include <ctime>
#include <iomanip>
#include <sys/time.h>

static bool headerNameEquals(const std::string &left, const std::string &right);
static std::string trimHeaderValue(const std::string &value);

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

static void addCgiHeaderToResponse(Response &response,
    const std::string &line)
{
    size_t      colon;
    std::string name;
    std::string value;

    colon = line.find(':');
    if (colon == std::string::npos)
        return ;
    name = line.substr(0, colon);
    value = trimHeaderValue(line.substr(colon + 1));
    if (name.empty())
        return ;
    response.addHeader(name, value);
}

static void addCgiHeadersToResponse(Response &response,
    const std::string &headers)
{
    std::istringstream stream;
    std::string        line;

    stream.str(headers);
    while (std::getline(stream, line))
    {
        if (!line.empty() && line[line.length() - 1] == '\r')
            line.erase(line.length() - 1);
        if (!line.empty())
            addCgiHeaderToResponse(response, line);
    }
}

static Response buildCgiResponse(const std::string &cgiOutput)
{
    Response    response;
    std::string separator;
    size_t      bodyStart;
    std::string cgiHeaders;
    std::string cgiBody;

    separator = "\r\n\r\n";
    bodyStart = cgiOutput.find(separator);
    if (bodyStart == std::string::npos)
    {
        separator = "\n\n";
        bodyStart = cgiOutput.find(separator);
    }
    response.setStatusCode(200);
    if (bodyStart != std::string::npos)
    {
        cgiHeaders = cgiOutput.substr(0, bodyStart);
        cgiBody = cgiOutput.substr(bodyStart + separator.length());
        response.setBody(cgiBody);
        addCgiHeadersToResponse(response, cgiHeaders);
    }
    else
    {
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

static std::string longToString(long value)
{
    std::ostringstream stream;

    stream << value;
    return (stream.str());
}

static std::string getDirectoryName(const std::string &path)
{
    size_t slash;

    slash = path.find_last_of('/');
    if (slash == std::string::npos)
        return (".");
    if (slash == 0)
        return ("/");
    return (path.substr(0, slash));
}

static std::string getRequestTime(void)
{
    return (longToString(static_cast<long>(std::time(NULL))));
}

static std::string getRequestTimeFloat(void)
{
    struct timeval time;
    std::ostringstream stream;

    if (gettimeofday(&time, NULL) == -1)
        return (getRequestTime() + ".000000");
    stream << time.tv_sec << "."
           << std::setw(6) << std::setfill('0') << time.tv_usec;
    return (stream.str());
}

static bool isCgiExtensionBoundary(const std::string &path, size_t position, const std::string &extension)
{
    size_t end;

    end = position + extension.length();
    if (end == path.length())
        return (true);
    if (path[end] == '/')
        return (true);
    return (false);
}

static size_t findCgiExtensionPosition(const std::string &path, const std::string &extension)
{
    size_t position;

    position = path.find(extension);
    while (position != std::string::npos)
    {
        if (isCgiExtensionBoundary(path, position, extension))
            return (position);
        position = path.find(extension, position + 1);
    }
    return (std::string::npos);
}

static CgiResolvedPath resolveCgiPath(const Request &request, const RouteConfig &route)
{
    CgiResolvedPath info;
    std::string cleanPath;
    size_t position;
    size_t scriptEnd;

    cleanPath = getPathWithoutQuery(request.getPath());
    for (std::vector<CgiConfig>::const_iterator it = route.cgi.begin();
         it != route.cgi.end(); ++it)
    {
        position = findCgiExtensionPosition(cleanPath, it->extension);
        if (position != std::string::npos)
        {
            scriptEnd = position + it->extension.length();
            info.isCgi = true;
            info.executable = it->executable;
            info.scriptName = cleanPath.substr(0, scriptEnd);
            info.pathInfo = cleanPath.substr(scriptEnd);
            info.scriptPath = joinPaths(route.root,
                                        getPathInsideRoute(info.scriptName, route));
            if (!info.pathInfo.empty())
                info.pathTranslated = joinPaths(route.root, info.pathInfo);
            return (info);
        }
    }
    return (info);
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

	it = request.getHeaders().begin();
	while (it != request.getHeaders().end())
	{
		if (headerNameEquals(it->first, name))
			return (trimHeaderValue(it->second));
		it++;
	}
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

static void addStandardCgiVariables(CgiContext &context, const Request &request, const ServerConfig &server, const std::string &remoteAddr, const CgiResolvedPath &cgiPath)
{
    context.standard.values["AUTH_TYPE"] = "";
    context.standard.values["CONTENT_LENGTH"] = getContentLength(request);
    context.standard.values["CONTENT_TYPE"] = getHeaderValue(request, "Content-Type");
    context.standard.values["GATEWAY_INTERFACE"] = "CGI/1.1";
    context.standard.values["PATH_INFO"] = cgiPath.pathInfo;
    context.standard.values["PATH_TRANSLATED"] = cgiPath.pathTranslated;
    context.standard.values["QUERY_STRING"] = getQueryString(request.getPath());
    context.standard.values["REMOTE_ADDR"] = remoteAddr;
    context.standard.values["REMOTE_HOST"] = "";
    context.standard.values["REMOTE_IDENT"] = "";
    context.standard.values["REMOTE_USER"] = "";
    context.standard.values["REQUEST_METHOD"] = request.getMethod();
    context.standard.values["SCRIPT_NAME"] = cgiPath.scriptName;
    context.standard.values["SERVER_NAME"] = server.serverName;
    context.standard.values["SERVER_PORT"] = intToString(server.listen.port);
    context.standard.values["SERVER_PROTOCOL"] = request.getVersion();
    context.standard.values["SERVER_SOFTWARE"] = "webserv/1.0";
}

static void addImplementationCgiVariables(CgiContext &context, const Request &request, const RouteConfig &route, const CgiResolvedPath &cgiPath)
{
    context.standard.values["SCRIPT_FILENAME"] = cgiPath.scriptPath;
    context.standard.values["DOCUMENT_ROOT"] = route.root;
    context.standard.values["REQUEST_URI"] = request.getPath();
    context.standard.values["REQUEST_SCHEME"] = "http";
    context.standard.values["HTTPS"] = "off";
    context.standard.values["SERVER_ADMIN"] = "admin@localhost";
    context.standard.values["REDIRECT_STATUS"] = "200";
    context.standard.values["FCGI_ROLE"] = "RESPONDER";
    context.standard.values["PHP_SELF"] = cgiPath.scriptName + cgiPath.pathInfo;
    context.standard.values["PATH"] = "/usr/bin:/bin";
    context.standard.values["PWD"] = getDirectoryName(cgiPath.scriptPath);
    context.standard.values["REQUEST_TIME"] = getRequestTime();
    context.standard.values["REQUEST_TIME_FLOAT"] = getRequestTimeFloat();
}

static bool headerNameEquals(const std::string &left, const std::string &right)
{
	size_t i;

	if (left.length() != right.length())
		return (false);
	i = 0;
	while (i < left.length())
	{
		if (std::tolower(static_cast<unsigned char>(left[i])) != std::tolower(static_cast<unsigned char>(right[i])))
			return (false);
		i++;
	}
	return (true);
}

static bool isContentHeader(const std::string &name)
{
	if (headerNameEquals(name, "Content-Length"))
		return (true);
	if (headerNameEquals(name, "Content-Type"))
		return (true);
	return (false);
}

static std::string buildCgiHttpHeaderName(const std::string &name)
{
	std::string result;
	size_t i;

	result = "HTTP_";
	i = 0;
	while (i < name.length())
	{
		if (name[i] == '-')
			result += '_';
		else
			result += static_cast<char>(std::toupper(static_cast<unsigned char>(name[i])));
		i++;
	}
	return (result);
}

static void addHttpHeaderVariables(CgiContext &context, const Request &request)
{
	std::map<std::string, std::string>::const_iterator it;

	it = request.getHeaders().begin();
	while (it != request.getHeaders().end())
	{
		if (!isContentHeader(it->first))
		{
			context.httpHeaders.values[buildCgiHttpHeaderName(it->first)] = trimHeaderValue(it->second);
		}
		it++;
	}
}

static CgiContext buildCgiContext(const Request &request, const RouteConfig &route, const ServerConfig &server, const std::string &remoteAddr, const CgiResolvedPath &cgiPath)
{
    CgiContext context;

    context.executable = cgiPath.executable;
    context.scriptPath = cgiPath.scriptPath;
    context.requestBody = request.getBody();
    std::cout << "Request body for CGI:\n[" << context.requestBody << "]\n";
    addStandardCgiVariables(context, request, server, remoteAddr, cgiPath);
    addHttpHeaderVariables(context, request);
    addImplementationCgiVariables(context, request, route, cgiPath);
    return (context);
}

Response RequestHandler::handleRequest(const Request &request, const RouteConfig &route, const ServerConfig &server, const std::string &remoteAddr)
{
    // Generate a response
    Response response;

    CgiResolvedPath cgiPath;

    cgiPath = resolveCgiPath(request, route);
    if (cgiPath.isCgi)
    {
        CgiContext context = buildCgiContext(request, route, server, remoteAddr, cgiPath);
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
