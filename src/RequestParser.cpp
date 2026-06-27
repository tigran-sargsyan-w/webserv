#include "RequestParser.hpp"
#include "RequestLine.hpp"
#include "ChunkedDecoder.hpp"
#include "HttpMessageUtils.hpp"
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

static bool isChunkedRequest(const Request &request)
{
	std::map<std::string, std::string>::const_iterator it;
	std::string value;

	it = request.getHeaders().find("transfer-encoding");

	if (it == request.getHeaders().end())
		return (false);

	value = it->second;
	HttpMessageUtils::trimHeaderValue(value);
	value = toLowerCase(value);

	return (value == "chunked");
}

// static int parseRequestLine(std::string &requestLine, Request &request) {
//   if (requestLine.empty())
//     return (1);
//   if (requestLine[requestLine.length() - 1] == '\r')
//     requestLine.erase(requestLine.length() - 1);

//   std::istringstream lineStream(requestLine);
//   std::string method;
//   std::string path;
//   std::string version;
//   lineStream >> method >> path >> version;

//   request.setMethod(method);
//   request.setPath(path);
//   request.setVersion(version);

// //   struct stat st;
// //   stat(path.c_str(), &st);
// //  if (!(S_ISDIR(st.st_mode) || S_ISREG(st.st_mode)))
// //  {
// //    std::cout << "Invalid path in request!\n";
// //    return (1);
// //   }
//   return (0);
// }

static int	parseRequestLine(std::string &requestLine, Request &request)
{
	RequestLine	line;

	if (!line.parse(requestLine))
		return (1);
	request.setMethod(line.getMethod());
	request.setPath(line.getUri());
	request.setVersion(line.getVersion());
	return (0);
}

static void parseHeader(std::string &header, Request &request)
{
    std::string key;
    std::string value;

    if (!HttpMessageUtils::splitHeaderLine(header, key, value))
        return;

    HttpMessageUtils::trimLeft(value);

    request.addHeader(key, value);
}

int RequestParser::parse(const std::string &rawRequest, Request &req)
{
    size_t headerEnd;
    size_t bodyStart;
    std::string headerPart;
    std::string body;
    std::string requestLine;
    std::string headerLine;
    std::istringstream headerStream;
    size_t contentLength;

    if (rawRequest.empty())
        return (1);

    if (!HttpMessageUtils::findHeaderEnd(rawRequest, headerEnd, bodyStart))
        return (1);

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

		if (!ChunkedDecoder::decode(rawRequest, bodyStart, body, messageEnd))
			return (1);
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
