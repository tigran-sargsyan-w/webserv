#include "PathUtils.hpp"

namespace PathUtils
{
	/**
	 * @brief Joins two path parts into one path.
	 * @param left - Left path part.
	 * @param right - Right path part.
	 * @return Combined path string.
	 */
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

	/**
	 * @brief Returns the directory portion of a path.
	 * @param path - Input path.
	 * @return Parent directory path, or "." if none exists.
	 */
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

	/**
	 * @brief Extracts the file name from a path.
	 * @param path - Input path.
	 * @return File name part of the path.
	 */
	std::string getFileName(const std::string &path)
	{
		size_t lastSlash;

		lastSlash = path.find_last_of('/');
		if (lastSlash == std::string::npos)
			return (path);
		return (path.substr(lastSlash + 1));
	}
}