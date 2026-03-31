#include "RequestHandler.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>

RequestHandler::RequestHandler()
{

}

RequestHandler::~RequestHandler()
{

}

Response RequestHandler::handleStatic(const Request& request)
{
  Response res;

  std::string fullPath = "www" + request.getPath();
  if (fullPath == "www/")
    fullPath = "www/index.html";
  std::ifstream file(fullPath.c_str());
  if (!file)
  {
    std::cerr << "Resourse not found!" << std::endl; //TODO: return 404 response
  }
  if (file)
  {
    std::ostringstream oss;
    std::string line;
    res.setStatusCode("200");
    file >> line;
    std::cout << line << "\n";
    res.setBody(line);
  }
  return res;
}

Response RequestHandler::handleRequest(const Request& request)
{
    // Generate a response
    Response response;

    if (request.getMethod() == "GET")
    {
        if (request.getPath() == "/")
        {
            response = handleStatic(request);
        }
        else
        {
            response.setStatusCode("404");
            response.setBody("<html><body><h1>404 Not Found</h1></body></html>");
        }
    }
    else
    {
        response.setStatusCode("405");
        response.setBody("<html><body><h1>405 Method Not Allowed</h1></body></html>");
    }
    std::ostringstream oss;
    oss << response.getBody().length();
    response.addHeader("Content-Type", "text/html");
    response.addHeader("Connection", "close");
    response.addHeader("Content-Length", oss.str());
    return response;
}
