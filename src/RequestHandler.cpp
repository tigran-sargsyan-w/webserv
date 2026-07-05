#include "RequestHandler.hpp"
#include "CgiRequestHandler.hpp"
#include "Config.hpp"
#include "ErrorResponseHandler.hpp"
#include "HttpMethod.hpp"
#include "RedirectHandler.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include "SessionHandler.hpp"
#include "StaticFileHandler.hpp"
#include "UploadHandler.hpp"
#include "DeleteHandler.hpp"

#include <sstream>
#include <string>
#include <map>

RequestHandler::RequestHandler() {}

RequestHandler::RequestHandler(const RequestHandler &other) { (void)other; }

RequestHandler::~RequestHandler() {}

RequestHandler &RequestHandler::operator=(const RequestHandler &other)
{
	(void)other;
	return (*this);
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

static bool hasHeader(const Response &response, const std::string &name)
{
	std::map<std::string, std::string> headers = response.getHeaders();
	return (headers.find(name) != headers.end());
}

Response RequestHandler::handleRequest(const Request &request, const RouteConfig &route, const ServerConfig &server)
{
	Response response;

	HttpMethod method = parseHttpMethod(request.getMethod());
	Response check = validateMethod(route, method, server);

	// Handle redirects
	if (route.hasReturn)
		response = RedirectHandler::handle(route, server);
	else if (method == HTTP_UNKNOWN)
		response = ErrorResponseHandler::build(501, "Not Implemented", server);
	else if (check.getStatusCode() != 0)
		response = check;

	else if (server.clientMaxBodySize > 0 && request.getBody().size() > server.clientMaxBodySize)
		response = ErrorResponseHandler::build(413, "Payload Too Large: Body size exceeds limit", server);
	else if (SessionHandler::canHandle(request))
		response = SessionHandler::handle(request, server);
	// Handle CGI requests
	else if (CgiRequestHandler::isCgiRequest(request, route))
		response = ErrorResponseHandler::build(500, "Internal Server Error", server);
	// Reject requests that match a CGI route but aren't a valid CGI path
	else if (!route.cgi.empty())
		response = ErrorResponseHandler::build(403, "Forbidden", server);
	else
	{
		switch (method)
		{
			case HTTP_GET:
				response = StaticFileHandler::handle(request, route, server);
				break;
			case HTTP_POST:
				response = UploadHandler::handle(request, route, server);
				break;
			case HTTP_DELETE:
				response = DeleteHandler::handle(request, route, server);
				break;
			default:
				response = ErrorResponseHandler::build(500, "Internal Server Error", server);
		}
	}

	std::ostringstream oss;
	oss << response.getBody().length();

	if (!hasHeader(response, "Content-Type"))
		response.addHeader("Content-Type", "text/html");
	if (!hasHeader(response, "Connection"))
		response.addHeader("Connection", "close");
	response.addHeader("Content-Length", oss.str());

	return response;
}
