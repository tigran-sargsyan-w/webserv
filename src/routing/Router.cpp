#include "Router.hpp"
#include "UriUtils.hpp"

/**
 * @brief Checks whether a route path matches the requested path prefix.
 * @param routePath - configured route path
 * @param requestPath - normalized request path
 * @return true when the route applies to the request
 */
bool Router::matches(const std::string &routePath, const std::string &requestPath)
{
	if (routePath == "/")
		return (true);
	if (requestPath == routePath)
		return (true);
	if (requestPath.find(routePath) != 0)
		return (false);
	if (requestPath.length() > routePath.length() && requestPath[routePath.length()] == '/')
		return (true);
	return (false);
}

/**
 * @brief Finds the best matching route for the requested target.
 * Uses the longest matching route path and falls back to the first route.
 * @param server - active server configuration
 * @param requestTarget - raw request target from the URI
 * @return matched route configuration
 */
const RouteConfig &Router::resolve(const ServerConfig &server, const std::string &requestTarget)
{
	const RouteConfig *bestRoute;
	std::string requestPath;
	size_t bestLength;

	requestPath = UriUtils::getPathWithoutQuery(requestTarget);
	bestRoute = NULL;
	bestLength = 0;
	for (std::vector<RouteConfig>::const_iterator it = server.routes.begin();
		 it != server.routes.end(); ++it)
	{
		if (matches(it->path, requestPath) && it->path.length() > bestLength)
		{
			bestRoute = &(*it);
			bestLength = it->path.length();
		}
	}
	if (bestRoute != NULL)
		return (*bestRoute);
	return (server.routes.front());
}
