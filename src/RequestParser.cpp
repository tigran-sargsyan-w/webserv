#include "RequestParser.hpp"
#include "ChunkedDecoder.hpp"
#include "HttpMessageUtils.hpp"
#include "Logger.hpp"
#include "Request.hpp"

#include <sstream>
#include <string>
#include <map>

static void parseHeader(std::string &header, Request &request)
{
	std::string key;
	std::string value;

	if (!HttpMessageUtils::splitHeaderLine(header, key, value))
		return;

	HttpMessageUtils::trimLeft(value);

	request.addHeader(key, value);
}

int RequestParser::parse(const std::string &rawRequest, Request &req, const RequestInspection &inspection)
{
	std::string headerPart;
	std::string body;
	std::string headerLine;
	std::istringstream headerStream;
	size_t messageEnd;

	if (inspection.status != COMPLETED)
		return (1);

	req.setMethod(inspection.requestLine.getMethod());
	req.setPath(inspection.requestLine.getUri());
	req.setVersion(inspection.requestLine.getVersion());

	headerPart = rawRequest.substr(0, inspection.headerEnd);
	headerStream.str(headerPart);

	if (!std::getline(headerStream, headerLine))
		return (1);

	while (std::getline(headerStream, headerLine))
	{
		if (headerLine.empty() || headerLine == "\r")
			break;
		parseHeader(headerLine, req);
	}

	if (inspection.isChunked)
	{
		if (!ChunkedDecoder::decode(rawRequest, inspection.bodyStart, body, messageEnd))
		{
			return (1);
		}
	}
	else if (inspection.hasContentLength)
	{
		body = rawRequest.substr(inspection.bodyStart, inspection.contentLength);
	}
	else
	{
		body = rawRequest.substr(inspection.bodyStart);
	}

	req.setBody(body);

	if (Logger::isDebugEnabled())
	{
		Logger::debug() << "Parsed method: " << req.getMethod() << std::endl;
		Logger::debug() << "Parsed path: " << req.getPath() << std::endl;
		Logger::debug() << "Parsed version: " << req.getVersion() << std::endl;
		Logger::debug() << "Parsed body: [" << req.getBody() << "]" << std::endl;
		Logger::debug() << "Parsed headers:" << std::endl;
		for (std::map<std::string, std::string>::const_iterator it = req.getHeaders().begin();
			 it != req.getHeaders().end(); ++it)
		{
			Logger::debug() << it->first << " = [" << it->second << "]" << std::endl;
		}
	}

	if (req.getMethod().empty())
	{
		Logger::debug() << "Failed to parse request\n";
		return (1);
	}

	req.setValid();
	return (0);
}
