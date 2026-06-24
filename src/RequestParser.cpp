#include "RequestParser.hpp"
#include "ChunkedDecoder.hpp"
#include "Request.hpp"
#include "utils.hpp"
#include <sstream>
#include <string>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <iostream>
#include <map>

static bool getContentLength(const Request &request, size_t &contentLength)
{
    std::map<std::string, std::string>::const_iterator it;
    std::istringstream stream;

    it = request.getHeaders().find("content-length");
    if (it == request.getHeaders().end())
        return (false);

    stream.str(it->second);
    stream >> contentLength;

    if (stream.fail())
        return (false);

    return (true);
}

static void trimHeaderValue(std::string &value)
{
	while (!value.empty()
		&& (value[0] == ' ' || value[0] == '\t'))
	{
		value.erase(0, 1);
	}

	while (!value.empty()
		&& (value[value.length() - 1] == ' '
			|| value[value.length() - 1] == '\t'
			|| value[value.length() - 1] == '\r'))
	{
		value.erase(value.length() - 1);
	}
}

static bool isChunkedRequest(const Request &request)
{
	std::map<std::string, std::string>::const_iterator it;
	std::string value;

	it = request.getHeaders().find("transfer-encoding");

	if (it == request.getHeaders().end())
		return (false);

	value = it->second;
	trimHeaderValue(value);
	value = toLowerCase(value);

	return (value == "chunked");
}

static int parseRequestLine(std::string &requestLine, Request &request) {
  if (requestLine.empty())
    return (1);
  if (requestLine[requestLine.length() - 1] == '\r')
    requestLine.erase(requestLine.length() - 1);

  std::istringstream lineStream(requestLine);
  std::string method;
  std::string path;
  std::string version;
  lineStream >> method >> path >> version;

  request.setMethod(method);
  request.setPath(path);
  request.setVersion(version);

//   struct stat st;
//   stat(path.c_str(), &st);
//  if (!(S_ISDIR(st.st_mode) || S_ISREG(st.st_mode)))
//  {
//    std::cout << "Invalid path in request!\n";
//    return (1);
//   }
  return (0);
}

static void parseHeader(std::string &header, Request &request)
{
    std::string key;
    std::string value;
    size_t colonIndex;

    if (!header.empty() && header[header.length() - 1] == '\r')
        header.erase(header.length() - 1);

    colonIndex = header.find(":");
    if (colonIndex == std::string::npos)
        return;

    key = header.substr(0, colonIndex);
    value = header.substr(colonIndex + 1);

    while (!value.empty() && (value[0] == ' ' || value[0] == '\t'))
        value.erase(0, 1);

    request.addHeader(key, value);
}

int RequestParser::parse(const std::string &rawRequest, Request &req)
{
    size_t headerEnd;
    size_t bodyStart;
    size_t separatorLength;
    std::string headerPart;
    std::string body;
    std::string requestLine;
    std::string headerLine;
    std::istringstream headerStream;
    size_t contentLength;

    if (rawRequest.empty())
        return (1);

    headerEnd = rawRequest.find("\r\n\r\n");
    separatorLength = 4;

    if (headerEnd == std::string::npos)
    {
        headerEnd = rawRequest.find("\n\n");
        separatorLength = 2;
    }

    if (headerEnd == std::string::npos)
        return (1);

    bodyStart = headerEnd + separatorLength;
    headerPart = rawRequest.substr(0, headerEnd);

    headerStream.str(headerPart);

    if (!std::getline(headerStream, requestLine))
        return (1);

    if (parseRequestLine(requestLine, req))
        return (1);

    while (std::getline(headerStream, headerLine))
    {
        if (headerLine.empty() || headerLine == "\r")
            break;
        parseHeader(headerLine, req);
    }

    if (isChunkedRequest(req))
	{
		size_t messageEnd;

		if (!ChunkedDecoder::decode(
				rawRequest,
				bodyStart,
				body,
				messageEnd))
		{
			return (1);
		}
	}
	else
	{
		body = rawRequest.substr(bodyStart);

		contentLength = 0;

		if (getContentLength(req, contentLength))
		{
			if (body.length() > contentLength)
				body = body.substr(0, contentLength);
		}
	}

	req.setBody(body);

    std::cout << "Parsed method: " << req.getMethod() << std::endl;
    std::cout << "Parsed path: " << req.getPath() << std::endl;
    std::cout << "Parsed version: " << req.getVersion() << std::endl;
    std::cout << "Parsed body: [" << req.getBody() << "]" << std::endl;

    std::cout << "Parsed headers:" << std::endl;
    for (std::map<std::string, std::string>::const_iterator it = req.getHeaders().begin();
        it != req.getHeaders().end(); ++it)
    {
        std::cout << it->first << " = [" << it->second << "]" << std::endl;
    }

    if (req.getMethod().empty())
    {
        std::cout << "Failed to parse request\n";
        return (1);
    }

    req.setValid();
    return (0);
}
