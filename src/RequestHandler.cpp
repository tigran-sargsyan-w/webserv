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
    res.setBody("<html><body><h1>404 Not Found</h1></body></html>");
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

Response RequestHandler::handleRequest(const Request &request)
{
  // Generate a response
  Response response;

  if (request.getPath() == "/cgi-bin/hello.py")
  {
    Response response;
    std::string cgiOutput = CgiHandler::runCgi();

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
