#ifndef PATH_UTILS_HPP
#define PATH_UTILS_HPP

#include <string>

namespace PathUtils
{
	std::string join(const std::string &left, const std::string &right);
	std::string getDirectoryName(const std::string &path);
	std::string getFileName(const std::string &path);
}

#endif