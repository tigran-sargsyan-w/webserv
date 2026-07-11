#include "SessionHandler.hpp"

#include "ErrorResponseHandler.hpp"
#include "HttpMethod.hpp"
#include "PathUtils.hpp"
#include "TemplateRenderer.hpp"

#include <map>
#include <sstream>

/**
 * @brief Converts time_t value to string
 * @param value - time value to convert
 * @return string representation of time
 */
static std::string toStringTime(std::time_t value)
{
	std::ostringstream oss;

	oss << value;
	return (oss.str());
}

/**
 * @brief Converts unsigned long value to string
 * @param value - unsigned value to convert
 * @return string representation of value
 */
static std::string toStringUnsigned(unsigned long value)
{
	std::ostringstream oss;

	oss << value;
	return (oss.str());
}

/**
 * @brief Gets the template root directory from route or server config
 * @param route - route configuration
 * @param server - server configuration
 * @return template root path
 */
static std::string getTemplateRoot(const RouteConfig &route, const ServerConfig &server)
{
	if (!route.root.empty())
		return (route.root);
	return (server.root);
}

/**
 * @brief Removes query string from request path
 * @param path - path with possible query string
 * @return normalized path without query parameters
 */
std::string SessionHandler::normalizePath(const std::string &path)
{
	size_t queryPos = path.find('?');

	if (queryPos == std::string::npos)
		return (path);
	return (path.substr(0, queryPos));
}

/**
 * @brief Constructs the logout path for a session route
 * @param route - route configuration
 * @return logout path
 */
std::string SessionHandler::getLogoutPath(const RouteConfig &route)
{
	if (route.sessionPath == "/")
		return ("/logout");
	return (route.sessionPath + "/logout");
}

/**
 * @brief Checks if this handler can process the request
 * @param request - HTTP request
 * @param route - route configuration
 * @return true if handler can handle the request
 */
bool SessionHandler::canHandle(const Request &request, const RouteConfig &route)
{
	std::string path = normalizePath(request.getPath());

	if (!route.sessionEnable)
		return (false);
	return (path == route.sessionPath || path == getLogoutPath(route));
}

/**
 * @brief Extracts Cookie header from the request
 * @param request - HTTP request
 * @return Cookie header value or empty string
 */
std::string SessionHandler::getCookieHeader(const Request &request)
{
	const std::map<std::string, std::string> &headers = request.getHeaders();
	std::map<std::string, std::string>::const_iterator it = headers.find("cookie");

	if (it == headers.end())
		return ("");
	return (it->second);
}

/**
 * @brief Handles session-related HTTP requests
 * @param request - HTTP request
 * @param route - route configuration
 * @param server - server configuration
 * @return HTTP response
 */
Response SessionHandler::handle(const Request &request, const RouteConfig &route, const ServerConfig &server)
{
	HttpMethod method = parseHttpMethod(request.getMethod());
	std::string path = normalizePath(request.getPath());

	if (method != HTTP_GET)
	{
		Response errorRes = ErrorResponseHandler::build(405, "Method Not Allowed", server);
		errorRes.addHeader("Allow", "GET");
		return (errorRes);
	}
	if (path == getLogoutPath(route))
		return (handleLogout(request, route, server));
	return (handleSession(request, route, server));
}

/**
 * @brief Handles session page display and session management
 * @param request - HTTP request
 * @param route - route configuration
 * @param server - server configuration
 * @return HTTP response with session page
 */
Response SessionHandler::handleSession(const Request &request, const RouteConfig &route, const ServerConfig &server)
{
	bool created;
	SessionData session = SessionManager::getOrCreate(getCookieHeader(request), created);
	Response response;

	response.setStatusCode(200);
	response.setBody(buildSessionPage(session, created, route, server));
	response.addHeader("Content-Type", "text/html");
	response.addHeader("Set-Cookie", SessionManager::buildCookieHeader(session.id));
	return (response);
}

/**
 * @brief Handles session logout and cookie expiration
 * @param request - HTTP request
 * @param route - route configuration
 * @param server - server configuration
 * @return HTTP response with logout confirmation
 */
Response SessionHandler::handleLogout(const Request &request, const RouteConfig &route, const ServerConfig &server)
{
	bool destroyed = SessionManager::destroy(getCookieHeader(request));
	Response response;

	response.setStatusCode(200);
	response.setBody(buildLogoutPage(destroyed, route, server));
	response.addHeader("Content-Type", "text/html");
	response.addHeader("Set-Cookie", SessionManager::buildExpiredCookieHeader());
	return (response);
}

/**
 * @brief Builds HTML session page from template
 * @param session - session data
 * @param created - true if session was newly created
 * @param route - route configuration
 * @param server - server configuration
 * @return rendered HTML page
 */
std::string SessionHandler::buildSessionPage(const SessionData &session, bool created, const RouteConfig &route, const ServerConfig &server)
{
	std::string templatePath = PathUtils::join(getTemplateRoot(route, server), "session.html");
	TemplateRenderer::Variables variables;
	std::string body;
	std::string status;

	if (created)
		status = "new session created";
	else
		status = "existing session restored from Cookie header";
	variables["{{STATUS}}"] = TemplateRenderer::htmlEscape(status);
	variables["{{SESSION_ID}}"] = TemplateRenderer::htmlEscape(session.id);
	variables["{{VISIT_COUNT}}"] = toStringUnsigned(session.visitCount);
	variables["{{CREATED_AT}}"] = toStringTime(session.createdAt);
	variables["{{LAST_SEEN}}"] = toStringTime(session.lastSeen);
	variables["{{SESSION_PATH}}"] = TemplateRenderer::htmlEscape(route.sessionPath);
	variables["{{LOGOUT_PATH}}"] = TemplateRenderer::htmlEscape(getLogoutPath(route));
	body = TemplateRenderer::render(templatePath, variables);
	if (body.empty())
		return ("Session template missing\n");
	return (body);
}

/**
 * @brief Builds HTML logout confirmation page from template
 * @param destroyed - true if session was successfully destroyed
 * @param route - route configuration
 * @param server - server configuration
 * @return rendered HTML page
 */
std::string SessionHandler::buildLogoutPage(bool destroyed, const RouteConfig &route, const ServerConfig &server)
{
	std::string templatePath = PathUtils::join(getTemplateRoot(route, server), "session-logout.html");
	TemplateRenderer::Variables variables;
	std::string body;
	std::string message;

	if (destroyed)
		message = "The current server-side session was removed.";
	else
		message = "No active session was found, but the browser cookie was expired.";
	variables["{{MESSAGE}}"] = TemplateRenderer::htmlEscape(message);
	variables["{{SESSION_PATH}}"] = TemplateRenderer::htmlEscape(route.sessionPath);
	body = TemplateRenderer::render(templatePath, variables);
	if (body.empty())
		return ("Session logout template missing\n");
	return (body);
}
