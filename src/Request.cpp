#include "Request.hpp"
#include "utils.hpp"

Request::Request() : isCgi(false)  {}

Request::~Request() {}

void Request::addHeader(const std::string &key, const std::string &value)
{
	headers[toLowerCase(key)] = value;
}