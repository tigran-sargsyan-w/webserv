#include "RequestHandler.hpp"

#include <sys/stat.h>
#include <sys/time.h>

#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <utils.hpp>

#include "CgiRequestHandler.hpp"
#include "Config.hpp"
#include "ErrorResponseBuilder.hpp"
#include "HttpMethod.hpp"
#include "RedirectHandler.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include "StaticFileHandler.hpp"

RequestHandler::RequestHandler () {}

RequestHandler::~RequestHandler () {}

static std::string joinPaths (const std::string &left, const std::string &right)
{
	if (left.empty ())
		return (right);
	if (right.empty ())
		return (left);
	if (left[left.length () - 1] == '/' && right[0] == '/')
		return (left + right.substr (1));
	if (left[left.length () - 1] != '/' && right[0] != '/')
		return (left + "/" + right);
	return (left + right);
}

static bool isPathSafe (const std::string &path)
{
	std::stringstream ss (path);
	std::string segment;

	while (std::getline (ss, segment, '/'))
	{
		if (segment == "..")
			return false;
	}
	return true;
}

static std::string getSafeUploadPath (const Request &request, const RouteConfig &route)
{
	std::string fileName = request.getPath ();
	size_t lastSlash = fileName.find_last_of ('/');

	if (lastSlash != std::string::npos)
		fileName = fileName.substr (lastSlash + 1);
	if (fileName.empty () || !isPathSafe (fileName))
		return "";
	if (route.uploadStore.empty ())
		return "";
	return joinPaths (route.uploadStore, fileName);
}

static Response buildSuccessResponse (int successCode, const std::string &successMessage)
{
	Response res;

	res.setStatusCode (successCode);
	res.setBody ("<html><body><h1>" + successMessage + "</h1></body></html>");
	return res;
}

static Response validateMethod (const RouteConfig &route, HttpMethod method)
{
	if (route.methods.find (method) == route.methods.end ())
	{
		Response errorRes = ErrorResponseBuilder::build (405, "Method Not Allowed");

		std::string allowedStr;
		for (std::set<HttpMethod>::iterator it = route.methods.begin ();
			 it != route.methods.end (); ++it)
		{
			if (it != route.methods.begin ())
				allowedStr += ", ";
			allowedStr += httpMethodToString (*it);
		}
		errorRes.addHeader ("Allow", allowedStr);
		return errorRes;
	}

	Response ok;
	ok.setStatusCode (0);
	return ok;
}

static bool hasHeader (const Response &response, const std::string &name)
{
	std::map<std::string, std::string> headers = response.getHeaders ();
	return (headers.find (name) != headers.end ());
}

Response RequestHandler::handleStatic (const Request &request, const RouteConfig &route)
{
	return (StaticFileHandler::handle (request, route));
}

Response RequestHandler::handlePost (const Request &request, const RouteConfig &route)
{
	// 1. Check the activation of the upload
	if (!route.uploadEnable)
		return ErrorResponseBuilder::build (403, "Forbidden: Upload is disabled for this route");

	// 2. Check the presence of a storage folder
	if (route.uploadStore.empty ())
		return ErrorResponseBuilder::build (500, "Internal Server Error: Upload store not configured");

	// 3. Check the body
	if (request.getBody ().empty ())
		return ErrorResponseBuilder::build (400, "Bad request: Empty body");

	// 4. Extraction and safety check of the filename
	std::string fullPath = getSafeUploadPath (request, route);
	if (fullPath.empty ())
		return ErrorResponseBuilder::build (400, "Bad Request: Invalid file name");

	// 5. Check if it's a folder
	struct stat pathStat;
	if (stat (fullPath.c_str (), &pathStat) == 0 && S_ISDIR (pathStat.st_mode))
		return ErrorResponseBuilder::build (409, "Conflict: A directory with this name already exists");

	// TODO: check that the body size isn't bigger than the route's client max body size

	// 6. Try to write the file
	std::ofstream file (fullPath.c_str (), std::ios::out | std::ios::binary);
	if (!file.is_open ())
		return ErrorResponseBuilder::build (500, "Internal Server Error: Could not open file");

	file << request.getBody ();
	file.close ();

	// 7. Success response
	return buildSuccessResponse (201, "Created: File uploaded");
}

Response RequestHandler::handleHttpDelete (const Request &request, const RouteConfig &route)
{
	// 1. Check that the path is safe and create the complete path
	std::string fullPath = getSafeUploadPath (request, route);
	if (fullPath.empty ())
		return ErrorResponseBuilder::build (403, "Forbidden: Invalid path sequence");

	// 2. Check if the resource exists
	struct stat pathStat;
	if (stat (fullPath.c_str (), &pathStat) != 0)
		return ErrorResponseBuilder::build (404, "Not Found: Resource does not exist");

	// 3. Forbid the deletion of folders
	if (S_ISDIR (pathStat.st_mode))
		return ErrorResponseBuilder::build (403, "Forbidden: Cannot delete a directory");

	// 4. Try to delete
	if (std::remove (fullPath.c_str ()) == 0)
		return buildSuccessResponse (200, "File deleted successfully");

	return ErrorResponseBuilder::build (500, "Internal Server Error: Failed to delete the file");
}

Response RequestHandler::handleRequest (const Request &request, const RouteConfig &route, const ServerConfig &server, const std::string &remoteAddr)
{
	Response response;

	// Handle redirects first
	if (route.hasReturn)
		return (RedirectHandler::handle (route));

	// Handle CGI requests
	if (CgiRequestHandler::isCgiRequest (request, route))
		return (CgiRequestHandler::handle (request, route, server, remoteAddr));

	// Reject requests that match a CGI route but aren't a valid CGI path
	if (!route.cgi.empty ())
		return (ErrorResponseBuilder::build (403, "Forbidden"));

	// Validate HTTP method
	HttpMethod method = parseHttpMethod (request.getMethod ());
	if (method == HTTP_UNKNOWN)
		return ErrorResponseBuilder::build (501, "Not Implemented");

	Response check = validateMethod (route, method);
	if (check.getStatusCode () != 0)
		return check;

	switch (method)
	{
		case HTTP_GET:
			response = RequestHandler::handleStatic (request, route);
			break;
		case HTTP_POST:
			response = RequestHandler::handlePost (request, route);
			break;
		case HTTP_DELETE:
			response = RequestHandler::handleHttpDelete (request, route);
			break;
		default:
			return ErrorResponseBuilder::build (500, "Internal Server Error");
	}

	std::ostringstream oss;
	oss << response.getBody ().length ();

	if (!hasHeader (response, "Content-Type"))
		response.addHeader ("Content-Type", "text/html");
	if (!hasHeader (response, "Connection"))
		response.addHeader ("Connection", "close");
	response.addHeader ("Content-Length", oss.str ());

	return response;
}
