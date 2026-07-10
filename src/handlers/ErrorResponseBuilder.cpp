#include "ErrorResponseBuilder.hpp"
#include "utils.hpp"

#include <sstream>
#include <string>

ErrorResponseBuilder::ErrorResponseBuilder () {}

ErrorResponseBuilder::ErrorResponseBuilder (const ErrorResponseBuilder &other)
{
	(void)other;
}

ErrorResponseBuilder &ErrorResponseBuilder::operator= (const ErrorResponseBuilder &other)
{
	(void)other;
	return (*this);
}

ErrorResponseBuilder::~ErrorResponseBuilder () {}

std::string ErrorResponseBuilder::buildDefaultBody(int statusCode, const std::string &message)
{
    std::ostringstream body;

    body << "<html>";
    body << "<head><title>";
    body << statusCode << " " << message;
    body << "</title></head>";
    body << "<body>";
    body << "<h1>" << statusCode << " " << message << "</h1>";
    body << "</body>";
    body << "</html>";
    return (body.str());
}

Response ErrorResponseBuilder::buildFromBody(int statusCode, const std::string &body)
{
    Response response;

    response.setStatusCode(statusCode);
    response.setBody(body);
    response.addHeader("Content-Type", "text/html");
    response.addHeader("Content-Length", intToString(body.length()));
    response.addHeader("Connection", "close");
    return (response);
}

Response ErrorResponseBuilder::build(int statusCode, const std::string &message)
{
    return (buildFromBody(statusCode, buildDefaultBody(statusCode, message)));
}