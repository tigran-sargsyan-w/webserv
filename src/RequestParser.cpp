#include <string>
#include "RequestParser.hpp"
#include "Request.hpp"

static Request RequestParser::parse(const std::string& rawRequest)
{
    Request request;

    std::istringstream ss;
    getline(ss, rawRequest);
    ss >> request._method >> request._path >> request._version;

    std::cout << "Method: " << request._method << "\n";
    std::cout << "Path: " << request._path << "\n";
    std::cout << "Version: " << request._version << "\n";
    return Request(rawRequest);
}
