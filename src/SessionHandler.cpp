#include "SessionHandler.hpp"

#include "ErrorResponseHandler.hpp"
#include "HttpMethod.hpp"

#include <map>
#include <sstream>

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
		return (handleLogout(request));
	return (handleSession(request));
}

Response SessionHandler::handleSession(const Request &request)
{
	bool created;
	SessionData session = SessionManager::getOrCreate(getCookieHeader(request), created);
	Response response;

	response.setStatusCode(200);
	response.setBody(buildSessionPage(session, created, RouteConfig()));
	response.addHeader("Content-Type", "text/html");
	response.addHeader("Set-Cookie", SessionManager::buildCookieHeader(session.id));
	return (response);
}

Response SessionHandler::handleLogout(const Request &request)
{
	bool destroyed = SessionManager::destroy(getCookieHeader(request));
	Response response;

	response.setStatusCode(200);
	response.setBody(buildLogoutPage(destroyed, RouteConfig()));
	response.addHeader("Content-Type", "text/html");
	response.addHeader("Set-Cookie", SessionManager::buildExpiredCookieHeader());
	return (response);
}

std::string SessionHandler::buildSessionPage(const SessionData &session, bool created, const RouteConfig &route)
{
	std::ostringstream body;
	std::string sessionPath = route.sessionPath.empty() ? "/session" : route.sessionPath;
	std::string logoutPath = route.sessionPath.empty() ? "/logout" : getLogoutPath(route);

	body << "<!DOCTYPE html>\n";
	body << "<html><head><title>webserv session demo</title></head><body>\n";
	body << "<h1>Cookies and sessions demo</h1>\n";
	if (created)
		body << "<p>Status: new session created.</p>\n";
	else
		body << "<p>Status: existing session restored from Cookie header.</p>\n";
	body << "<ul>\n";
	body << "<li>Session id: " << session.id << "</li>\n";
	body << "<li>Visit count: " << session.visitCount << "</li>\n";
	body << "<li>Created at: " << session.createdAt << "</li>\n";
	body << "<li>Last seen: " << session.lastSeen << "</li>\n";
	body << "</ul>\n";
	body << "<p><a href=\"" << sessionPath << "\">Refresh session</a></p>\n";
	body << "<p><a href=\"" << logoutPath << "\">Destroy session</a></p>\n";
	body << "</body></html>\n";
	return (body.str());
}

std::string SessionHandler::buildLogoutPage(bool destroyed, const RouteConfig &route)
{
	std::ostringstream body;
	std::string sessionPath = route.sessionPath.empty() ? "/session" : route.sessionPath;

	body << "<!DOCTYPE html>\n";
	body << "<html><head><title>webserv logout</title></head><body>\n";
	body << "<h1>Session destroyed</h1>\n";
	if (destroyed)
		body << "<p>The current server-side session was removed.</p>\n";
	else
		body << "<p>No active session was found, but the browser cookie was expired.</p>\n";
	body << "<p><a href=\"" << sessionPath << "\">Create a new session</a></p>\n";
	body << "</body></html>\n";
	return (body.str());
}
