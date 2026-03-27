#include <string>
#include <sstream>
#include <iostream>
#include "RequestParser.hpp"
#include "Request.hpp"

static void parseRequestLine(std::string& requestLine, Request& request)
{
    if (requestLine.empty())
        return;
    if (requestLine.back() == '\r')
        requestLine.pop_back();

    std::istringstream lineStream(requestLine);
    std::string method;
    std::string path;
    std::string version;
    lineStream >> method >> path >> version;

    request.setMethod(method);
    request.setPath(path);
    request.setVersion(version);
}

static void parseHeader(std::istringstream& header,Request& request)
{
    (void) request;
    std::string key;
    std::string value;

    header >> key >> value;
    if (key.back() == ':')
        key.pop_back();
    else
        return;
    if (value.back() == '\r')
        value.pop_back();
    std::cout << "key: " << key << "value: " << value << "\n";
    request.addHeader(key, value);
    return;
}

Request RequestParser::parse(const std::string& rawRequest)
{
    Request request;

    std::istringstream ss(rawRequest);
    std::string requestLine;
    std::getline(ss, requestLine);
    if (requestLine.empty())
        return request;
    if (requestLine.back() == '\r')
        requestLine.pop_back();
    parseRequestLine(requestLine, request);

    std::string headerLine;
    while (std::getline(ss, headerLine) && headerLine != "\r\n" && !headerLine.empty())
    {
        std::istringstream ss(headerLine);
        parseHeader(ss, request);
    }
   //TODO: parse body if Content-Length is present
    return request;
}
