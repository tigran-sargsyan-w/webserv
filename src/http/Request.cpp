#include "Request.hpp"
#include "Utils.hpp"

Request::Request() : valid(false), isCgi(false) {}

Request::~Request() {}

void Request::addHeader(const std::string &key, const std::string &value)
{
	headers[toLowerCase(key)] = value;
}
