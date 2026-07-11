#include "DeleteHandler.hpp"
#include "ErrorResponseHandler.hpp"
#include "StoragePathResolver.hpp"

#include <cstdio>
#include <sys/stat.h>
#include <string>

/**
 * @brief Builds a response for a successful file deletion.
 * @return HTTP 200 response.
 */
static Response buildDeletedResponse()
{
	Response response;

	response.setStatusCode(200);
	response.setBody("<html><body><h1>File deleted successfully</h1></body></html>");
	return (response);
}

/**
 * @brief Handles delete requests.
 * Validates permissions, checks the target path, and removes the file.
 * @param request - Incoming request.
 * @param route - Matched route configuration.
 * @param server - Server configuration.
 * @return HTTP response for success or failure.
 */
Response DeleteHandler::handle(const Request &request, const RouteConfig &route, const ServerConfig &server)
{
	std::string fullPath;
	struct stat pathStat;

	// 1. Check the activation of the upload
	if (!route.uploadEnable)
		return (ErrorResponseHandler::build(403, "Forbidden: Delete is disabled for this route", server));

	// 2. Check that the path is safe and create the complete path
	fullPath = StoragePathResolver::resolve(request, route);
	if (fullPath.empty())
		return (ErrorResponseHandler::build(403, "Forbidden: Invalid path sequence", server));

	// 3. Check if the resource exists
	if (stat(fullPath.c_str(), &pathStat) != 0)
		return (ErrorResponseHandler::build(404, "Not Found: Resource does not exist", server));

	// 4. Forbid the deletion of folders
	if (S_ISDIR(pathStat.st_mode))
		return (ErrorResponseHandler::build(403, "Forbidden: Cannot delete a directory", server));

	// 5. Try to delete
	if (std::remove(fullPath.c_str()) != 0)
		return (ErrorResponseHandler::build(500, "Internal Server Error: Failed to delete the file", server));

	return (buildDeletedResponse());
}
