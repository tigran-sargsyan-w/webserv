#ifndef SESSIONHANDLER_HPP
#define SESSIONHANDLER_HPP

#include "Config.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include "SessionManager.hpp"

#include <string>

class SessionHandler
{
	public:
		static bool canHandle(const Request &request, const RouteConfig &route);
		static Response handle(const Request &request, const RouteConfig &route, const ServerConfig &server);

	private:
		SessionHandler();
		SessionHandler(const SessionHandler &other);
		SessionHandler &operator=(const SessionHandler &other);
		~SessionHandler();

		static std::string getCookieHeader(const Request &request);
		static std::string normalizePath(const std::string &path);
		static std::string getLogoutPath(const RouteConfig &route);
		static Response handleSession(const Request &request, const RouteConfig &route, const ServerConfig &server);
		static Response handleLogout(const Request &request, const RouteConfig &route, const ServerConfig &server);
		static std::string buildSessionPage(const SessionData &session, bool created, const RouteConfig &route, const ServerConfig &server);
		static std::string buildLogoutPage(bool destroyed, const RouteConfig &route, const ServerConfig &server);
};

#endif
