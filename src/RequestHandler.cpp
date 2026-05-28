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
#include <unistd.h>
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

	if (decoded.find ("/../") != std::string::npos || decoded.find ("/..") == decoded.length () - 3)
		return "";

	std::string fileName = getFileName (decoded);

	if (fileName.empty () || fileName == ".." || fileName == ".")
		return "";

	struct stat storeStat;
	if (stat (route.uploadStore.c_str (), &storeStat) != 0 || !S_ISDIR (storeStat.st_mode))
		return "";

	if (access (route.uploadStore.c_str (), W_OK) != 0)
		return "";

	std::string storePath = route.uploadStore;
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

static Response validateMethod (const RouteConfig &route, HttpMethod method, const ServerConfig &server)
{
	if (route.methods.find (method) == route.methods.end ())
	{
		Response errorRes = ErrorResponseHandler::build (405, "Method Not Allowed", server);

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

Response RequestHandler::handleStatic (const Request &request, const RouteConfig &route, const ServerConfig &server)
{
	return (StaticFileHandler::handle (request, route, server));
}

Response RequestHandler::handlePost (const Request &request, const RouteConfig &route, const ServerConfig &server)
{
	Response response;

	// 1. Check the activation of the upload
	if (!route.uploadEnable)
		response = ErrorResponseHandler::build (403, "Forbidden: Upload is disabled for this route", server);

	// 2. Check the presence of a storage folder
	else if (route.uploadStore.empty ())
		response = ErrorResponseHandler::build (500, "Internal Server Error: Upload store not configured", server);

	// 3. Check the body
	else if (request.getBody ().empty ())
		response = ErrorResponseHandler::build (400, "Bad request: Empty body", server);

	else
	{
		// 4. Extraction and safety check of the filename
		std::string fullPath = getSafeUploadPath (request, route);
		if (fullPath.empty ())
			response = ErrorResponseHandler::build (400, "Bad Request: Invalid file name", server);
		else
		{
			// 5. Check if a directory with this name already exists
			struct stat pathStat;
			if (stat (fullPath.c_str (), &pathStat) == 0 && S_ISDIR (pathStat.st_mode))
				response = ErrorResponseHandler::build (409, "Conflict: A directory with this name already exists", server);
			else
			{
				// 6. Try to open the file for writing
				std::ofstream file (fullPath.c_str (), std::ios::out | std::ios::binary);
				if (!file.is_open ())
					response = ErrorResponseHandler::build (500, "Internal Server Error: Could not open file", server);
				else
				{
					// 7. Write the body data
					file.write (request.getBody ().data (), request.getBody ().size ());
					file.close ();

					// 8. Check if disk write failed (disk full)
					if (file.fail ())
					{
						std::remove (fullPath.c_str ());
						response = ErrorResponseHandler::build (500, "Internal Server Error: Write failed", server);
					}
					// 9. Success response
					else
						response = buildSuccessResponse (201, "Created: File uploaded");
				}
			}
		}
	}
	return response;
}

Response RequestHandler::handleHttpDelete (const Request &request, const RouteConfig &route, const ServerConfig &server)
{
	Response response;

	// 1. Check the activation of the upload
	if (!route.uploadEnable)
		response = ErrorResponseHandler::build (403, "Forbidden: Delete is disabled for this route", server);
	else
	{
		// 2. Check that the path is safe and create the complete path
		std::string fullPath = getSafeUploadPath (request, route);
		if (fullPath.empty ())
			response = ErrorResponseHandler::build (403, "Forbidden: Invalid path sequence", server);
		else
		{
			// 3. Check if the resource exists
			struct stat pathStat;
			if (stat (fullPath.c_str (), &pathStat) != 0)
				response = ErrorResponseHandler::build (404, "Not Found: Resource does not exist", server);

			// 4. Forbid the deletion of folders
			else if (S_ISDIR (pathStat.st_mode))
				response = ErrorResponseHandler::build (403, "Forbidden: Cannot delete a directory", server);
			else
			{
				// 5. Try to delete
				if (std::remove (fullPath.c_str ()) == 0)
					response = buildSuccessResponse (200, "File deleted successfully");
				else
					response = ErrorResponseHandler::build (500, "Internal Server Error: Failed to delete the file", server);
			}
		}
	}
	return response;
}

Response RequestHandler::handleRequest (const Request &request, const RouteConfig &route, const ServerConfig &server)
{
	std::cout << "--- [DEBUG] handleRequest ENTRÉ ! ---" << std::endl;
	Response response;

	HttpMethod method = parseHttpMethod (request.getMethod ());
	Response check = validateMethod (route, method, server);

	// Handle redirects
	if (route.hasReturn)
		response = RedirectHandler::handle (route, server);
	else if (method == HTTP_UNKNOWN)
		response = ErrorResponseHandler::build (501, "Not Implemented", server);
	else if (check.getStatusCode () != 0)
		response = check;

	else if (server.clientMaxBodySize > 0 && request.getBody ().size () > server.clientMaxBodySize)
		response = ErrorResponseHandler::build (413, "Payload Too Large: Body size exceeds limit", server);
	// Handle CGI requests
	else if (CgiRequestHandler::isCgiRequest (request, route))
		response = ErrorResponseHandler::build (500, "Internal Server Error", server);
	// Reject requests that match a CGI route but aren't a valid CGI path
	else if (!route.cgi.empty ())
		response = ErrorResponseHandler::build (403, "Forbidden", server);
	else
	{
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
				response = ErrorResponseHandler::build (500, "Internal Server Error", server);
		}
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