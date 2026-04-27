#include "ConfigValidator.hpp"
#include "ConfigDebug.hpp"

#include <iostream>
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

void ConfigValidator::validate(const Config &config)
{
	if (config.servers.empty())
		throw configError("at least one server block is required");

	for (size_t serverIndex = 0; serverIndex < config.servers.size(); ++serverIndex)
	{
		const ServerConfig &server = config.servers[serverIndex];
		if (server.listen.port <= 0 || server.listen.port > 65535)
			throw configError("server " + toString(static_cast<int>(serverIndex)) + " has invalid listen port");
		if (server.listen.host.empty())
			throw configError("server " + toString(static_cast<int>(serverIndex)) + " has empty listen host");
		if (server.root.empty())
			throw configError("server " + toString(static_cast<int>(serverIndex)) + " requires root");

		for (size_t routeIndex = 0; routeIndex < server.routes.size(); ++routeIndex)
		{
			const RouteConfig &route = server.routes[routeIndex];
			if (route.path.empty() || route.path[0] != '/')
				throw configError("server " + toString(static_cast<int>(serverIndex)) + " has location with invalid path");
			if (route.methods.empty())
				throw configError("location " + route.path + " has no allowed methods");
			if (route.uploadEnable && route.uploadStore.empty())
				throw configError("location " + route.path + " has upload_enable on but upload_store is missing");
			if (route.hasReturn && (route.returnCode < 100 || route.returnCode > 599))
				throw configError("location " + route.path + " has invalid return status code");
		}
	}
}

void ConfigValidator::debugPrintValidation(const Config &config)
{
	std::cout << ConfigDebug::validator << "[validator] config is valid" << ConfigDebug::reset << "\n";
	std::cout << ConfigDebug::validator << "  servers: " << config.servers.size() << ConfigDebug::reset << "\n";
	for (size_t serverIndex = 0; serverIndex < config.servers.size(); ++serverIndex)
	{
		const ServerConfig &server = config.servers[serverIndex];
		std::cout << ConfigDebug::validator << "  server #" << serverIndex << " routes: " << server.routes.size() << ConfigDebug::reset << "\n";
	}
}
