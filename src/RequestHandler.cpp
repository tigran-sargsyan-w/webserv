#include "RequestHandler.hpp"
#include "RequestParser.hpp"

RequestHandler::RequestHandler()
{

}

RequestHandler::~RequestHandler()
{

}

Response RequestHandler::handleRequest(const Request& request)
{
    // Generate a response
    Response response;
    if (request.getMethod() == "GET")
    {
        if (request.getPath() == "/")
        {
            response.setStatusCode("200");
            response.setBody("<html><body><h1>Hello from WebServ</h1></body></html>");
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

    return response;
}
