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

static void debugPrintParsedRequest(const Request &request)
{
	std::map<std::string, std::string>::const_iterator it;

	if (!Logger::isDebugEnabled())
		return;
	Logger::debug() << "Parsed method: " << request.getMethod() << std::endl;
	Logger::debug() << "Parsed path: " << request.getPath() << std::endl;
	Logger::debug() << "Parsed version: " << request.getVersion() << std::endl;
	Logger::debug() << "Parsed body: [" << request.getBody() << "]" << std::endl;
	Logger::debug() << "Parsed headers:" << std::endl;
	it = request.getHeaders().begin();
	while (it != request.getHeaders().end())
	{
		Logger::debug() << it->first << " = [" << it->second << "]" << std::endl;
		++it;
	}
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
	debugPrintParsedRequest(req);

	if (req.getMethod().empty())
	{
		Logger::debug() << "Failed to parse request\n";
		return (1);
	}

	return (0);
}
