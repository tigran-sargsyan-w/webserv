#include "SessionHandler.hpp"

#include "ErrorResponseHandler.hpp"
#include "HttpMethod.hpp"

#include <fstream>
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

static std::string joinPath(const std::string &root, const std::string &fileName)
{
	if (root.empty())
		return (fileName);
	if (root[root.length() - 1] == '/')
		return (root + fileName);
	return (root + "/" + fileName);
}

static std::string getTemplateRoot(const RouteConfig &route, const ServerConfig &server)
{
	if (!route.root.empty())
		return (route.root);
	return (server.root);
}

static std::string readTemplateFile(const std::string &path)
{
	std::ifstream file(path.c_str());
	std::ostringstream buffer;

	if (!file.is_open())
		return ("");
	buffer << file.rdbuf();
	return (buffer.str());
}

static void replaceAll(std::string &text, const std::string &from, const std::string &to)
{
	size_t pos = 0;

	if (from.empty())
		return;
	while ((pos = text.find(from, pos)) != std::string::npos)
	{
		text.replace(pos, from.length(), to);
		pos += to.length();
	}
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
	std::string templatePath = joinPath(getTemplateRoot(route, server), "session.html");
	std::string body = readTemplateFile(templatePath);
	std::string status;

	if (body.empty())
		return ("Session template missing\n");
	if (created)
		status = "new session created";
	else
		status = "existing session restored from Cookie header";
	replaceAll(body, "{{STATUS}}", status);
	replaceAll(body, "{{SESSION_ID}}", session.id);
	replaceAll(body, "{{VISIT_COUNT}}", toStringUnsigned(session.visitCount));
	replaceAll(body, "{{CREATED_AT}}", toStringTime(session.createdAt));
	replaceAll(body, "{{LAST_SEEN}}", toStringTime(session.lastSeen));
	replaceAll(body, "{{SESSION_PATH}}", route.sessionPath);
	replaceAll(body, "{{LOGOUT_PATH}}", getLogoutPath(route));
	return (body);
}

std::string SessionHandler::buildLogoutPage(bool destroyed, const RouteConfig &route, const ServerConfig &server)
{
	std::string templatePath = joinPath(getTemplateRoot(route, server), "session-logout.html");
	std::string body = readTemplateFile(templatePath);
	std::string message;

	if (body.empty())
		return ("Session logout template missing\n");
	if (destroyed)
		message = "The current server-side session was removed.";
	else
		message = "No active session was found, but the browser cookie was expired.";
	replaceAll(body, "{{MESSAGE}}", message);
	replaceAll(body, "{{SESSION_PATH}}", route.sessionPath);
	return (body);
}
