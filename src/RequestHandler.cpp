#include "RequestHandler.hpp"

#include <sys/stat.h>
#include <sys/time.h>

#include <climits>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <utils.hpp>

#include "CgiRequestHandler.hpp"
#include "Config.hpp"
#include "ErrorResponseHandler.hpp"
#include "HttpMethod.hpp"
#include "RedirectHandler.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include "StaticFileHandler.hpp"

RequestHandler::RequestHandler () {}

RequestHandler::~RequestHandler () {}

/******************** UTILS ********************/

static std::string getPathWithoutQuery (const std::string &path)
{
	size_t questionMark;

	questionMark = path.find ('?');
	if (questionMark == std::string::npos)
		return (path);
	return (path.substr (0, questionMark));
}

/***********************************************/

static bool isHexDigit (char c)
{
	return ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
}

static int hexToInt (char c)
{
	if (c >= '0' && c <= '9')
		return (c - '0');
	if (c >= 'a' && c <= 'f')
		return (c - 'a' + 10);
	return (c - 'A' + 10);
}

static std::string urlDecodePath (const std::string &path)
{
	std::string result;
	size_t i = 0;

	while (i < path.length ())
	{
		if (path[i] == '%' && i + 2 < path.length () && isHexDigit (path[i + 1]) && isHexDigit (path[i + 2]))
		{
			result += static_cast<char> (hexToInt (path[i + 1]) * 16 + hexToInt (path[i + 2]));
			i += 3;
		}
		else
		{
			result += path[i];
			i++;
		}
	}
	return result;
}

static std::string getFileName (const std::string &path)
{
	size_t lastSlash = path.find_last_of ('/');

	if (lastSlash == std::string::npos)
		return (path);
	return (path.substr (lastSlash + 1));
}

static std::string getSafeUploadPath (const Request &request, const RouteConfig &route)
{
	if (route.uploadStore.empty ())
		return "";

	std::string decoded = urlDecodePath (getPathWithoutQuery (request.getPath ()));

	if (decoded.find ('\0') != std::string::npos)
		return "";
	if (decoded.find ("..") != std::string::npos)
		return "";

	std::string fileName = getFileName (decoded);

	if (fileName.empty () || fileName.find ('/') != std::string::npos)
		return "";

	char resolvedStore[PATH_MAX];
	if (realpath (route.uploadStore.c_str (), resolvedStore) == NULL)
		return "";

	std::string storePath (resolvedStore);
	if (storePath[storePath.length () - 1] != '/')
		storePath += '/';

	return (storePath + fileName);
}

static Response buildSuccessResponse (int successCode, const std::string &successMessage)
{
	Response res;

	res.setStatusCode (successCode);
	res.setBody ("<html><body><h1>" + successMessage + "</h1></body></html>");
	return res;
}

static Response validateMethod(const RouteConfig &route, HttpMethod method, const ServerConfig &server)
{
	if (route.methods.find(method) == route.methods.end())
	{
		Response errorRes = ErrorResponseHandler::build(405, "Method Not Allowed", server);

		std::string allowedStr;
		for (std::set<HttpMethod>::iterator it = route.methods.begin();
			 it != route.methods.end(); ++it)
		{
			if (it != route.methods.begin())
				allowedStr += ", ";
			allowedStr += httpMethodToString(*it);
		}
		errorRes.addHeader("Allow", allowedStr);
		return errorRes;
	}

	Response ok;
	ok.setStatusCode(0);
	return ok;
}

static bool hasHeader (const Response &response, const std::string &name)
{
	std::map<std::string, std::string> headers = response.getHeaders ();
	return (headers.find (name) != headers.end ());
}

Response RequestHandler::handleStatic (const Request &request, const RouteConfig &route, const ServerConfig &server)
{
	return (StaticFileHandler::handle (request, route, server));
}

Response RequestHandler::handlePost (const Request &request, const RouteConfig &route, const ServerConfig &server)
{
	// 1. Check the activation of the upload
	if (!route.uploadEnable)
		return ErrorResponseHandler::build (403, "Forbidden: Upload is disabled for this route", server);

	// 2. Check the presence of a storage folder
	if (route.uploadStore.empty ())
		return ErrorResponseHandler::build (500, "Internal Server Error: Upload store not configured", server);

	// 3. Check the body
	if (request.getBody ().empty ())
		return ErrorResponseHandler::build (400, "Bad request: Empty body", server);

	// 4. Extraction and safety check of the filename
	std::string fullPath = getSafeUploadPath (request, route);
	if (fullPath.empty ())
		return ErrorResponseHandler::build (400, "Bad Request: Invalid file name", server);

	// 5. Check if it's a folder
	struct stat pathStat;
	if (stat (fullPath.c_str (), &pathStat) == 0 && S_ISDIR (pathStat.st_mode))
		return ErrorResponseHandler::build (409, "Conflict: A directory with this name already exists", server);

	// TODO: check that the body size isn't bigger than the route's client max body size

	// 6. Try to write the file
	std::ofstream file (fullPath.c_str (), std::ios::out | std::ios::binary);
	if (!file.is_open ())
		return ErrorResponseHandler::build (500, "Internal Server Error: Could not open file", server);

	file << request.getBody ();
	file.close ();

	// 7. Success response
	return buildSuccessResponse (201, "Created: File uploaded");
}

Response RequestHandler::handleHttpDelete (const Request &request, const RouteConfig &route, const ServerConfig &server)
{
	// 1. Check that the path is safe and create the complete path
	std::string fullPath = getSafeUploadPath (request, route);
	if (fullPath.empty ())
		return ErrorResponseHandler::build (403, "Forbidden: Invalid path sequence", server);

	// 2. Check if the resource exists
	struct stat pathStat;
	if (stat (fullPath.c_str (), &pathStat) != 0)
		return ErrorResponseHandler::build (404, "Not Found: Resource does not exist", server);

	// 3. Forbid the deletion of folders
	if (S_ISDIR (pathStat.st_mode))
		return ErrorResponseHandler::build (403, "Forbidden: Cannot delete a directory", server);

	// 4. Try to delete
	if (std::remove (fullPath.c_str ()) == 0)
		return buildSuccessResponse (200, "File deleted successfully");

	return ErrorResponseHandler::build (500, "Internal Server Error: Failed to delete the file", server);
}

Response RequestHandler::handleRequest (const Request &request, const RouteConfig &route, const ServerConfig &server, const std::string &remoteAddr)
{
	(void)remoteAddr;
	Response response;

	// Handle redirects first
	if (route.hasReturn)
		return (RedirectHandler::handle (route, server));

	// Handle CGI requests
	if (CgiRequestHandler::isCgiRequest(request, route))
    	return (ErrorResponseHandler::build(500, "Internal Server Error", server));

	// Reject requests that match a CGI route but aren't a valid CGI path
	if (!route.cgi.empty ())
		return (ErrorResponseHandler::build (403, "Forbidden", server));

	// Validate HTTP method
	HttpMethod method = parseHttpMethod (request.getMethod ());
	if (method == HTTP_UNKNOWN)
		return ErrorResponseHandler::build (501, "Not Implemented", server);

	Response check = validateMethod (route, method, server);
	if (check.getStatusCode () != 0)
		return check;

	switch (method)
	{
		case HTTP_GET:
			response = RequestHandler::handleStatic (request, route, server);
			break;
		case HTTP_POST:
			response = RequestHandler::handlePost (request, route, server);
			break;
		case HTTP_DELETE:
			response = RequestHandler::handleHttpDelete (request, route, server);
			break;
		default:
			return ErrorResponseHandler::build (500, "Internal Server Error", server);
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
