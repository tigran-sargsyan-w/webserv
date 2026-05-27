#include "RequestInspector.hpp"

#include <string>
#include <sstream>

const std::size_t MAX_HEADERS_SIZE = 32768;
const std::size_t MAX_REQUEST_LINE_SIZE = 8192;
// const std::size_t MAX_HEADER_FIELD_SIZE = 8192;

static size_t findHeaderEnd(const std::string &rawRequest, size_t &bodyStart)
{
	size_t headerEnd;

	headerEnd = rawRequest.find("\r\n\r\n");
	if (headerEnd != std::string::npos)
	{
		bodyStart = headerEnd + 4;
		return (headerEnd);
	}

	headerEnd = rawRequest.find("\n\n");
	if (headerEnd != std::string::npos)
	{
		bodyStart = headerEnd + 2;
		return (headerEnd);
	}

	bodyStart = std::string::npos;
	return (std::string::npos);
}

static void trimLeft(std::string &value)
{
	while (!value.empty() && (value[0] == ' ' || value[0] == '\t'))
		value.erase(0, 1);
}

static bool parseSize(const std::string &text, size_t &value)
{
	std::istringstream stream;
	char extra;

	stream.str(text);
	stream >> value;
	if (stream.fail())
		return (false);
	if (stream >> extra)
		return (false);
	return (true);
}

static bool getContentLength(const std::string &headers, size_t &contentLength)
{
	std::istringstream stream;
	std::string line;
	std::string key;
	std::string value;
	size_t colon;

	stream.str(headers);
	while (std::getline(stream, line))
	{
		if (!line.empty() && line[line.length() - 1] == '\r')
			line.erase(line.length() - 1);

		colon = line.find(':');
		if (colon == std::string::npos)
			continue;

		key = line.substr(0, colon);
		value = line.substr(colon + 1);
		trimLeft(value);

		if (key == "Content-Length")
			return (parseSize(value, contentLength));
	}
	return (false);
}

void RequestInspector::inspectRequestLine(const std::string &requestLine)
{
	std::stringstream ss;
	std::string method;
	std::string uri;
	std::string version;
	std::string extra;

	if (requestLine.size() > MAX_REQUEST_LINE_SIZE)
	{
		this->status = URI_TOO_LONG;
		return;
	}

	ss << requestLine;
	if (!(ss >> method >> uri >> version))
	{
		this->status = BAD_REQUEST;
		return;
	}

	if (ss >> extra)
	{
		this->status = BAD_REQUEST;
		return;
	}

	if (method != "GET" && method != "POST" && method != "DELETE")
	{
		this->status = NOT_IMPLEMENTED;
		return;
	}

	if (uri.empty() || uri[0] != '/')
	{
		this->status = BAD_REQUEST;
		return;
	}

	if (version != "HTTP/1.1" && version != "HTTP/1.0")
	{
		this->status = BAD_REQUEST;
		return;
	}

	requestLineValid = true;
	this->status = COMPLETED;
}

InspectRequestStatus RequestInspector::inspectRequest(const std::string &rawRequest, size_t maxBodySize)
{
	std::string requestLine;
	std::stringstream ss;
	size_t headerEnd;
	size_t bodyStart;
	size_t contentLength;
	size_t currentBodySize;
	std::string headers;

	if (rawRequest.empty())
	{
		this->status = NEED_MORE_DATA;
		return (this->status);
	}

	ss << rawRequest;
	if (!std::getline(ss, requestLine))
	{
		this->status = NEED_MORE_DATA;
		return (this->status);
	}

	inspectRequestLine(requestLine);
	if (this->status != COMPLETED)
		return (this->status);

	headerEnd = findHeaderEnd(rawRequest, bodyStart);
	if (headerEnd == std::string::npos)
	{
		this->status = NEED_MORE_DATA;
		return (this->status);
	}

	if (headerEnd > MAX_HEADERS_SIZE)
	{
		this->status = HEADER_TOO_LARGE;
		return (this->status);
	}

	headers = rawRequest.substr(0, headerEnd);
	contentLength = 0;

	if (getContentLength(headers, contentLength))
	{
		if (contentLength > maxBodySize)
		{
			this->status = REQUEST_TOO_LARGE;
			return (this->status);
		}

		currentBodySize = rawRequest.length() - bodyStart;
		if (currentBodySize < contentLength)
		{
			this->status = NEED_MORE_DATA;
			return (this->status);
		}
	}

	this->status = COMPLETED;
	return (this->status);
}