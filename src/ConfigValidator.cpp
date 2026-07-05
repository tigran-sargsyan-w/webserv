#include "ConfigValidator.hpp"
#include "ConfigDebug.hpp"

#include <cctype>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

static std::runtime_error configError(const std::string &message)
{
	return (std::runtime_error("Config validation error: " + message));
}

static std::string toString(int value)
{
	std::ostringstream oss;
	oss << value;
	return (oss.str());
}

static bool isRedirectStatusCode(int code)
{
	return (code == 301 || code == 302 || code == 303 || code == 307 || code == 308);
}

static bool isErrorStatusCode(int code)
{
	return (code >= 400 && code <= 599);
}

static bool hasNullByte(const std::string &value)
{
	return (value.find('\0') != std::string::npos);
}

static bool hasWhiteSpace(const std::string &value)
{
	for (size_t index = 0; index < value.length(); ++index)
	{
		if (std::isspace(static_cast<unsigned char>(value[index])))
			return (true);
	}
	return (false);
}

static bool isValidConfigPath(const std::string &path)
{
	if (path.empty())
		return (false);
	if (hasNullByte(path))
		return (false);
	return (true);
}

static bool isValidOptionalConfigPath(const std::string &path)
{
	if (path.empty())
		return (true);
	if (hasNullByte(path))
		return (false);
	return (true);
}

static bool isValidLocationPath(const std::string &path)
{
	if (path.empty())
		return (false);
	if (hasNullByte(path))
		return (false);
	if (path[0] != '/')
		return (false);
	if (path.length() > 1 && path[1] == '/')
		return (false);
	return (true);
}

static bool isValidRedirectTarget(const std::string &target)
{
	if (!isValidLocationPath(target))
		return (false);
	return (true);
}

static bool isPathInsideRoute(const std::string &path, const std::string &routePath)
{
	if (routePath == "/")
		return (true);
	if (path == routePath)
		return (true);
	if (path.find(routePath + "/") == 0)
		return (true);
	return (false);
}

static bool isValidCgiExtension(const std::string &extension)
{
	if (extension.length() < 2)
		return (false);
	if (hasNullByte(extension))
		return (false);
	if (hasWhiteSpace(extension))
		return (false);
	if (extension[0] != '.')
		return (false);
	if (extension.find('/') != std::string::npos)
		return (false);
	return (true);
}

static bool isValidCgiExecutablePath(const std::string &path)
{
	if (!isValidConfigPath(path))
		return (false);
	if (hasWhiteSpace(path))
		return (false);
	if (path[0] != '/')
		return (false);
	if (path[path.length() - 1] == '/')
		return (false);
	return (true);
}

static std::string buildServerKey(const ServerConfig &server)
{
	std::ostringstream oss;

	oss << server.listen.host << ":" << server.listen.port << ":" << server.serverName;
	return (oss.str());
}

static void validateServerUniqueness(const Config &config)
{
	std::set<std::string> keys;

	for (size_t serverIndex = 0; serverIndex < config.servers.size(); ++serverIndex)
	{
		const ServerConfig &server = config.servers[serverIndex];
		std::string key = buildServerKey(server);

		if (keys.find(key) != keys.end())
		{
			throw configError("duplicate server block for " + server.listen.host + ":" + toString(server.listen.port) + " with server_name '" + server.serverName + "'");
		}
		keys.insert(key);
	}
}

static void validateErrorPages(const ServerConfig &server, size_t serverIndex)
{
	std::map<int, std::string>::const_iterator it;

	for (it = server.errorPages.begin(); it != server.errorPages.end(); ++it)
	{
		if (!isErrorStatusCode(it->first))
			throw configError("server " + toString(static_cast<int>(serverIndex)) + " has invalid error_page code: " + toString(it->first));
		if (!isValidConfigPath(it->second))
			throw configError("server " + toString(static_cast<int>(serverIndex)) + " has invalid error_page path for code " + toString(it->first));
	}
}

static void validateCgiConfig(const RouteConfig &route)
{
	std::set<std::string> extensions;
	std::vector<CgiConfig>::const_iterator it;

	for (it = route.cgi.begin(); it != route.cgi.end(); ++it)
	{
		if (!isValidCgiExtension(it->extension))
			throw configError("location " + route.path + " has invalid CGI extension");
		if (!isValidCgiExecutablePath(it->executable))
			throw configError("location " + route.path + " has invalid CGI executable path");
		if (extensions.find(it->extension) != extensions.end())
			throw configError("location " + route.path + " has duplicate CGI extension: " + it->extension);
		extensions.insert(it->extension);
	}
}

static void validateUploadStoreDirectory(const RouteConfig &route)
{
	struct stat pathStat;
	if (stat(route.uploadStore.c_str(), &pathStat) != 0)
		throw configError("location " + route.path + ": upload_store " + route.uploadStore + " does not exist");
	else if (!S_ISDIR(pathStat.st_mode))
		throw configError("location " + route.path + ": upload_store " + route.uploadStore + " is not a directory");
	else if (access(route.uploadStore.c_str(), W_OK | X_OK) != 0)
		throw configError("location " + route.path + ": upload_store " + route.uploadStore + " is not accessible or writable");
}

static void validateRoutePaths(const RouteConfig &route)
{
	if (!isValidLocationPath(route.path))
		throw configError("location has invalid path: " + route.path);
	if (!isValidOptionalConfigPath(route.root))
		throw configError("location " + route.path + " has invalid root path");
	if (!isValidOptionalConfigPath(route.index))
		throw configError("location " + route.path + " has invalid index path");
	if (!isValidOptionalConfigPath(route.uploadStore))
		throw configError("location " + route.path + " has invalid upload_store path");
	if (!route.sessionPath.empty() && !isValidLocationPath(route.sessionPath))
		throw configError("location " + route.path + " has invalid session_path");
}

static void validateSessionConfig(const RouteConfig &route)
{
	if (route.sessionEnable && route.sessionPath.empty())
		throw configError("location " + route.path + " has session_enable on but session_path is missing");
	if (!route.sessionEnable && !route.sessionPath.empty())
		throw configError("location " + route.path + " has session_path but session_enable is off");
	if (!route.sessionPath.empty() && !isPathInsideRoute(route.sessionPath, route.path))
		throw configError("location " + route.path + " has session_path outside of the location prefix");
	if (route.sessionEnable && route.methods.find(HTTP_GET) == route.methods.end())
		throw configError("location " + route.path + " has session_enable on but GET is not allowed");
}

static void validateRoute(const RouteConfig &route)
{
	validateRoutePaths(route);
	if (route.methods.empty())
		throw configError("location " + route.path + " has no allowed methods");
	if (route.uploadEnable && route.uploadStore.empty())
		throw configError("location " + route.path + " has upload_enable on but upload_store is missing");
	if (!route.uploadEnable && !route.uploadStore.empty())
		throw configError("location " + route.path + " has upload_store but upload_enable is off");
	if (route.uploadEnable)
		validateUploadStoreDirectory(route);
	validateSessionConfig(route);
	if (route.hasReturn && !isRedirectStatusCode(route.returnCode))
		throw configError("location " + route.path + " has invalid redirect status code");
	if (route.hasReturn && !isValidRedirectTarget(route.returnPath))
		throw configError("location " + route.path + " has invalid redirect target");
	validateCgiConfig(route);
}

static void validateRoutes(const ServerConfig &server, size_t serverIndex)
{
	std::set<std::string> paths;

	if (server.routes.empty())
		throw configError("server " + toString(static_cast<int>(serverIndex)) + " requires at least one location");

	for (size_t routeIndex = 0; routeIndex < server.routes.size(); ++routeIndex)
	{
		const RouteConfig &route = server.routes[routeIndex];

		if (paths.find(route.path) != paths.end())
			throw configError("server " + toString(static_cast<int>(serverIndex)) + " has duplicate location: " + route.path);
		paths.insert(route.path);
		validateRoute(route);
	}
}

static void validateServerPaths(const ServerConfig &server, size_t serverIndex)
{
	if (!isValidConfigPath(server.root))
		throw configError("server " + toString(static_cast<int>(serverIndex)) + " has invalid root path");
	if (!isValidOptionalConfigPath(server.index))
		throw configError("server " + toString(static_cast<int>(serverIndex)) + " has invalid index path");
}

static void validateServer(const ServerConfig &server, size_t serverIndex)
{
	if (server.listen.port <= 0 || server.listen.port > 65535)
		throw configError("server " + toString(static_cast<int>(serverIndex)) + " has invalid listen port");
	if (server.listen.host.empty())
		throw configError("server " + toString(static_cast<int>(serverIndex)) + " has empty listen host");
	validateServerPaths(server, serverIndex);
	if (server.clientMaxBodySize == 0)
		throw configError("server " + toString(static_cast<int>(serverIndex)) + " requires client_max_body_size greater than 0");
	if (server.clientTimeout <= 0)
		throw configError("server " + toString(static_cast<int>(serverIndex)) + " requires client_timeout greater than 0");
	if (server.clientTimeout > 3600)
		throw configError("server " + toString(static_cast<int>(serverIndex)) + " requires client_timeout at most 3600");
	validateErrorPages(server, serverIndex);
	validateRoutes(server, serverIndex);
}

void ConfigValidator::validate(const Config &config)
{
	if (config.servers.empty())
		throw configError("at least one server block is required");
	validateServerUniqueness(config);
	for (size_t serverIndex = 0; serverIndex < config.servers.size(); ++serverIndex)
		validateServer(config.servers[serverIndex], serverIndex);
}

void ConfigValidator::debugPrintValidation(const Config &config)
{
	std::cout << ConfigDebug::validator << "[validator] config is valid"
			  << ConfigDebug::reset << "\n";
	std::cout << ConfigDebug::validator << "  servers: "
			  << config.servers.size() << ConfigDebug::reset << "\n";
	for (size_t serverIndex = 0; serverIndex < config.servers.size(); ++serverIndex)
	{
		const ServerConfig &server = config.servers[serverIndex];

		std::cout << ConfigDebug::validator << "  server #"
				  << serverIndex << " routes: " << server.routes.size()
				  << ConfigDebug::reset << "\n";
	}
}
