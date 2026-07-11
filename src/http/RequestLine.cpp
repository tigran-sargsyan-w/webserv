#include "RequestLine.hpp"

#include <cstddef>
#include <sstream>

namespace
{
	const std::size_t MAX_REQUEST_LINE_SIZE = 8192;
}

/**
 * @brief Create a request-line object with a default bad-request status.
 */
RequestLine::RequestLine()
{
	this->status = REQUEST_LINE_BAD_REQUEST;
}

/**
 * @brief Reset all stored request-line fields.
 */
void RequestLine::clear(void)
{
	this->method.clear();
	this->uri.clear();
	this->version.clear();
	this->status = REQUEST_LINE_BAD_REQUEST;
}

/**
 * @brief Parse and validate a raw request line.
 */
bool RequestLine::parse(const std::string &line)
{
	std::stringstream stream;
	std::string extra;

	this->clear();
	if (line.size() > MAX_REQUEST_LINE_SIZE)
	{
		this->status = REQUEST_LINE_URI_TOO_LONG;
		return (false);
	}
	stream << line;
	if (!(stream >> this->method >> this->uri >> this->version))
	{
		this->status = REQUEST_LINE_BAD_REQUEST;
		return (false);
	}
	if (stream >> extra)
	{
		this->status = REQUEST_LINE_BAD_REQUEST;
		return (false);
	}
	if (!this->isSupportedMethod())
	{
		this->status = REQUEST_LINE_NOT_IMPLEMENTED;
		return (false);
	}
	if (!this->isValidUri())
	{
		this->status = REQUEST_LINE_BAD_REQUEST;
		return (false);
	}
	if (!this->isSupportedVersion())
	{
		this->status = REQUEST_LINE_BAD_REQUEST;
		return (false);
	}
	this->status = REQUEST_LINE_OK;
	return (true);
}

/**
 * @brief Check whether the request method is supported.
 */
bool RequestLine::isSupportedMethod(void) const
{
	return (this->method == "GET" || this->method == "POST" || this->method == "DELETE");
}

/**
 * @brief Validate the URI format.
 */
bool RequestLine::isValidUri(void) const
{
	return (!this->uri.empty() && this->uri[0] == '/');
}

/**
 * @brief Check whether the HTTP version is supported.
 */
bool RequestLine::isSupportedVersion(void) const
{
	return (this->version == "HTTP/1.1" || this->version == "HTTP/1.0");
}

/**
 * @brief Get the current request-line parsing status.
 */
RequestLineStatus RequestLine::getStatus(void) const
{
	return (this->status);
}

/**
 * @brief Get the parsed HTTP method.
 */
const std::string &RequestLine::getMethod(void) const
{
	return (this->method);
}

/**
 * @brief Get the parsed request URI.
 */
const std::string &RequestLine::getUri(void) const
{
	return (this->uri);
}

/**
 * @brief Get the parsed HTTP version.
 */
const std::string &RequestLine::getVersion(void) const
{
	return (this->version);
}
