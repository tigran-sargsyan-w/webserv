#include "CookieParser.hpp"

#include <cctype>
#include <cstddef>

/**
 * @brief Removes leading and trailing whitespace from a string
 * @param value - string to trim
 * @return trimmed string
 */
std::string CookieParser::trim(const std::string &value)
{
	size_t start = 0;
	size_t end = value.size();

	while (start < end && std::isspace(static_cast<unsigned char>(value[start])))
		++start;
	while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])))
		--end;
	return (value.substr(start, end - start));
}

/**
 * @brief Parses HTTP Cookie header into a map of name-value pairs
 * @param cookieHeader - raw Cookie header value
 * @return map of cookie names to values
 */
std::map<std::string, std::string> CookieParser::parse(const std::string &cookieHeader)
{
	std::map<std::string, std::string> cookies;
	size_t start = 0;

	while (start <= cookieHeader.size())
	{
		size_t end = cookieHeader.find(';', start);
		std::string part;
		size_t equalPos;

		if (end == std::string::npos)
			part = cookieHeader.substr(start);
		else
			part = cookieHeader.substr(start, end - start);
		part = trim(part);
		equalPos = part.find('=');
		if (equalPos != std::string::npos)
		{
			std::string name = trim(part.substr(0, equalPos));
			std::string value = trim(part.substr(equalPos + 1));

			if (!name.empty())
				cookies[name] = value;
		}
		if (end == std::string::npos)
			break;
		start = end + 1;
	}
	return (cookies);
}

/**
 * @brief Extracts a specific cookie value from the Cookie header
 * @param cookieHeader - raw Cookie header value
 * @param name - cookie name to retrieve
 * @return cookie value or empty string if not found
 */
std::string CookieParser::getCookieValue(const std::string &cookieHeader, const std::string &name)
{
	std::map<std::string, std::string> cookies = parse(cookieHeader);
	std::map<std::string, std::string>::const_iterator it = cookies.find(name);

	if (it == cookies.end())
		return ("");
	return (it->second);
}
