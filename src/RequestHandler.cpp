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

static std::string buildCgiScriptPath(const Request &request)
{
    return ("www" + getPathWithoutQuery(request.getPath()));
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

static bool isCgiPath(const std::string &path)
{
    return (path.find("/cgi-bin/") == 0);
}

Response RequestHandler::handleRequest(const Request &request)
{
  // Generate a response
  Response response;

  if (isCgiPath(request.getPath()))
  {
    std::vector<CgiConfig> cgiConfigs;
    CgiConfig pythonCgi;
    pythonCgi.extension = ".py";
    pythonCgi.executable = "/usr/bin/python3";
    cgiConfigs.push_back(pythonCgi);

    std::string scriptPath = buildCgiScriptPath(request);
    std::string queryString = getQueryString(request.getPath());
    std::string executable = getCgiExecutable(request.getPath(), cgiConfigs);

    if (executable.empty())
    {
        Response response;
        response.setStatusCode(403);
        response.setBody("<html><body><h1>403 Forbidden</h1></body></html>");
        response.addHeader("Content-Type", "text/html");
        response.addHeader("Content-Length", intToString(response.getBody().length()));
        response.addHeader("Connection", "close");
        return (response);
    }

    std::string cgiOutput = CgiHandler::runCgi(executable, scriptPath, queryString);
    return (buildCgiResponse(cgiOutput));
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
