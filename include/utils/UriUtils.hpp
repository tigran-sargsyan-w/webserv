#ifndef URI_UTILS_HPP
#define URI_UTILS_HPP

#include <string>

namespace UriUtils
{
	std::string getPathWithoutQuery(const std::string &uri);
	std::string getQueryString(const std::string &uri);
	std::string decodePath(const std::string &path);
}

#endif