#include "UploadHandler.hpp"
#include "ErrorResponseHandler.hpp"
#include "StoragePathResolver.hpp"

#include <cstdio>
#include <fstream>
#include <sys/stat.h>
#include <string>

static Response buildCreatedResponse()
{
	Response response;

	response.setStatusCode(201);
	response.setBody("<html><body><h1>Created: File uploaded</h1></body></html>");
	return (response);
}

static bool hasExplicitBodyFraming(const Request &request)
{
	const std::map<std::string, std::string> &headers = request.getHeaders();

	return (headers.find("content-length") != headers.end()
		|| headers.find("transfer-encoding") != headers.end());
}

static Response writeUploadedFile(const Request &request, const std::string &fullPath, const ServerConfig &server)
{
	std::ofstream file;

	// Try to open the file for writing
	file.open(fullPath.c_str(), std::ios::out | std::ios::binary);

	if (!file.is_open())
		return (ErrorResponseHandler::build(500, "Internal Server Error: Could not open file", server));

	// Write the body data
	file.write(request.getBody().data(), request.getBody().size());
	file.close();

	if (file.fail())
	{
		std::remove(fullPath.c_str());
		return (ErrorResponseHandler::build(500, "Internal Server Error: Write failed", server));
	}

	return (buildCreatedResponse());
}

Response UploadHandler::handle(const Request &request, const RouteConfig &route, const ServerConfig &server)
{
	std::string fullPath;
	struct stat pathStat;

	// 1. Check the activation of the upload
	if (!route.uploadEnable)
		return (ErrorResponseHandler::build(403, "Forbidden: Upload is disabled for this route", server));

	// 2. Check the presence of a storage folder
	if (route.uploadStore.empty())
		return (ErrorResponseHandler::build(500, "Internal Server Error: Upload store not configured", server));

	// 3. Check the body
	if (request.getBody().empty() && !hasExplicitBodyFraming(request))
		return (ErrorResponseHandler::build(400, "Bad request: Empty body", server));

	// 4. Extraction and safety check of the filename
	fullPath = StoragePathResolver::resolve(request, route);
	if (fullPath.empty())
		return (ErrorResponseHandler::build(400, "Bad Request: Invalid file name", server));

	// 5. Check if a directory with this name already exists
	if (stat(fullPath.c_str(), &pathStat) == 0 && S_ISDIR(pathStat.st_mode))
		return (ErrorResponseHandler::build(409, "Conflict: A directory with this name already exists", server));

	return (writeUploadedFile(request, fullPath, server));
}
