#include "Request.hpp"
#include "Utils.hpp"

/**
 * @brief Create an empty request.
 */
Request::Request() {}

/**
 * @brief Destroy the request.
 */
Request::~Request() {}

/**
 * @brief Store a header using a lowercase key.
 * @param key - Header name.
 * @param value - Header value.
 */
void Request::addHeader(const std::string &key, const std::string &value)
{
	headers[toLowerCase(key)] = value;
}
