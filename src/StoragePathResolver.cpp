#include "StoragePathResolver.hpp"

#include "PathUtils.hpp"
#include "UriUtils.hpp"

#include <sys/stat.h>
#include <unistd.h>

namespace StoragePathResolver
{
	static bool	hasNulByte(const std::string &path)
	{
		return (path.find('\0') != std::string::npos);
	}

	static bool	hasTraversalSequence(const std::string &path)
	{
		if (path.find("/../") != std::string::npos)
			return (true);
		if (path.length() >= 3
			&& path.find("/..") == path.length() - 3)
			return (true);
		return (false);
	}

	static bool	isSafeDecodedPath(const std::string &path)
	{
		if (hasNulByte(path))
			return (false);
		if (hasTraversalSequence(path))
			return (false);
		return (true);
	}

	static bool	isValidStorageFileName(const std::string &fileName)
	{
		if (fileName.empty())
			return (false);
		if (fileName == ".")
			return (false);
		if (fileName == "..")
			return (false);
		return (true);
	}

	static bool	isValidStorageDirectory(const std::string &path)
	{
		struct stat	pathStat;

		if (path.empty())
			return (false);
		if (stat(path.c_str(), &pathStat) != 0)
			return (false);
		if (!S_ISDIR(pathStat.st_mode))
			return (false);
		if (access(path.c_str(), W_OK) != 0)
			return (false);
		return (true);
	}

	std::string	resolve(const Request &request,
			const RouteConfig &route)
	{
		std::string	decodedPath;
		std::string	fileName;

		if (route.uploadStore.empty())
			return ("");

		decodedPath = UriUtils::decodePath(
			UriUtils::getPathWithoutQuery(request.getPath()));

		if (!isSafeDecodedPath(decodedPath))
			return ("");

		fileName = PathUtils::getFileName(decodedPath);
		if (!isValidStorageFileName(fileName))
			return ("");

		if (!isValidStorageDirectory(route.uploadStore))
			return ("");

		return (PathUtils::join(route.uploadStore, fileName));
	}
}
