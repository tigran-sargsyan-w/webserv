#include "SessionHandler.hpp"

#include "ErrorResponseHandler.hpp"
#include "HttpMethod.hpp"
#include "PathUtils.hpp"
#include "TemplateRenderer.hpp"

#include <map>
#include <sstream>

static std::string toStringTime(std::time_t value)
{
	std::ostringstream oss;

	oss << value;
	return (oss.str());
}

static std::string toStringUnsigned(unsigned long value)
{
	std::ostringstream oss;

	oss << value;
	return (oss.str());
}

static std::string getTemplateRoot(const RouteConfig &route, const ServerConfig &server)
{
	if (!route.root.empty())
		return (route.root);
	return (server.root);
}

std::string SessionHandler::normalizePath(const std::string &path)
{
	size_t queryPos = path.find('?');

	if (queryPos == std::string::npos)
		return (path);
	return (path.substr(0, queryPos));
}

std::string SessionHandler::getLogoutPath(const RouteConfig &route)
{
	if (route.sessionPath == "/")
		return ("/logout");
	return (route.sessionPath + "/logout");
}

bool SessionHandler::canHandle(const Request &request, const RouteConfig &route)
{
	std::string path = normalizePath(request.getPath());

	if (!route.sessionEnable)
		return (false);
	return (path == route.sessionPath || path == getLogoutPath(route));
}

std::string SessionHandler::getCookieHeader(const Request &request)
{
	const std::map<std::string, std::string> &headers = request.getHeaders();
	std::map<std::string, std::string>::const_iterator it = headers.find("cookie");

	if (it == headers.end())
		return ("");
	return (it->second);
}

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
