#include "PathUtils.hpp"

namespace PathUtils
{
	std::string join(const std::string &left, const std::string &right)
	{
		if (left.empty())
			return (right);
		if (right.empty())
			return (left);

		if (left[left.length() - 1] == '/' && right[0] == '/')
			return (left + right.substr(1));
		if (left[left.length() - 1] != '/' && right[0] != '/')
			return (left + "/" + right);

		return (left + right);
	}

	std::string getDirectoryName(const std::string &path)
	{
		size_t lastSlash;

		lastSlash = path.find_last_of('/');
		if (lastSlash == std::string::npos)
			return (".");
		if (lastSlash == 0)
			return ("/");
		return (path.substr(0, lastSlash));
	}

	std::string getFileName(const std::string &path)
	{
		size_t lastSlash;

		lastSlash = path.find_last_of('/');
		if (lastSlash == std::string::npos)
			return (path);
		return (path.substr(lastSlash + 1));
	}
}