#include "ErrorResponseBuilder.hpp"
#include "utils.hpp"

#include <sstream>
#include <string>

ErrorResponseBuilder::ErrorResponseBuilder() {}

ErrorResponseBuilder::ErrorResponseBuilder(const ErrorResponseBuilder &other)
{
    (void)other;
}

ErrorResponseBuilder &ErrorResponseBuilder::operator=(const ErrorResponseBuilder &other)
{
    (void)other;
    return (*this);
}

ErrorResponseBuilder::~ErrorResponseBuilder() {}

Response ErrorResponseBuilder::build(int statusCode, const std::string &message)
{
    Response response;
    std::stringstream stream;
    std::string body;

    response.setStatusCode(statusCode);

    stream << "<html><body><h1>"
           << statusCode
           << " "
           << message
           << "</h1></body></html>";

    body = stream.str();

    response.setBody(body);
    response.addHeader("Content-Type", "text/html");
    response.addHeader("Content-Length", intToString(body.length()));
    response.addHeader("Connection", "close");

    return (response);
}