#include "ConfigValidator.hpp"
#include "ConfigDebug.hpp"

#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

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

static bool isValidRedirectTarget(const std::string &target)
{
    if (target.empty())
        return (false);
    if (target[0] != '/')
        return (false);
    if (target.length() > 1 && target[1] == '/')
        return (false);
    return (true);
}

static bool isValidConfigPath(const std::string &path)
{
	if (path.empty())
		return (false);
	return (true);
}

static bool isValidCgiExtension(const std::string &extension)
{
	if (extension.length() < 2)
		return (false);
	if (extension[0] != '.')
		return (false);
	return (true);
}

static void validateErrorPages(const ServerConfig &server, size_t serverIndex)
{
	std::map<int, std::string>::const_iterator it;

	for (it = server.errorPages.begin(); it != server.errorPages.end(); ++it)
	{
		if (!isErrorStatusCode(it->first))
			throw configError("server " + toString(static_cast<int>(serverIndex)) + " has invalid error_page code: " + toString(it->first));
		if (!isValidConfigPath(it->second))
			throw configError("server " + toString(static_cast<int>(serverIndex)) + " has empty error_page path for code " + toString(it->first));
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
		if (it->executable.empty())
			throw configError("location " + route.path + " has empty CGI executable");
		if (extensions.find(it->extension) != extensions.end())
			throw configError("location " + route.path + " has duplicate CGI extension: " + it->extension);
		extensions.insert(it->extension);
	}
}

static void validateRoute(const RouteConfig &route, size_t serverIndex)
{
	if (route.path.empty() || route.path[0] != '/')
		throw configError("server " + toString(static_cast<int>(serverIndex)) + " has location with invalid path");
	if (route.methods.empty())
		throw configError("location " + route.path + " has no allowed methods");
	if (route.uploadEnable && route.uploadStore.empty())
		throw configError("location " + route.path + " has upload_enable on but upload_store is missing");
	if (!route.uploadEnable && !route.uploadStore.empty())
		throw configError("location " + route.path + " has upload_store but upload_enable is off");
	if (route.hasReturn && !isRedirectStatusCode(route.returnCode))
		throw configError("location " + route.path + " has invalid redirect status code");
	if (route.hasReturn && !isValidRedirectTarget(route.returnPath))
		throw configError("location " + route.path + " has invalid redirect target");
	validateCgiConfig(route);
}

static void validateRoutes(const ServerConfig &server, size_t serverIndex)
{
	std::set<std::string> paths;

	for (size_t routeIndex = 0; routeIndex < server.routes.size(); ++routeIndex)
	{
		const RouteConfig &route = server.routes[routeIndex];

		if (paths.find(route.path) != paths.end())
			throw configError("server " + toString(static_cast<int>(serverIndex)) + " has duplicate location: " + route.path);
		paths.insert(route.path);
		validateRoute(route, serverIndex);
	}
}

static void validateServer(const ServerConfig &server, size_t serverIndex)
{
	if (server.listen.port <= 0 || server.listen.port > 65535)
		throw configError("server " + toString(static_cast<int>(serverIndex)) + " has invalid listen port");
	if (server.listen.host.empty())
		throw configError("server " + toString(static_cast<int>(serverIndex)) + " has empty listen host");
	if (server.root.empty())
		throw configError("server " + toString(static_cast<int>(serverIndex)) + " requires root");
	if (server.clientMaxBodySize == 0)
		throw configError("server " + toString(static_cast<int>(serverIndex)) + " requires client_max_body_size greater than 0");
	validateErrorPages(server, serverIndex);
	validateRoutes(server, serverIndex);
}

void ConfigValidator::validate(const Config &config)
{
	if (config.servers.empty())
		throw configError("at least one server block is required");
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