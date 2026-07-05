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
		static bool canHandle(const Request &request);
		static Response handle(const Request &request, const ServerConfig &server);

	private:
		SessionHandler();
		SessionHandler(const SessionHandler &other);
		SessionHandler &operator=(const SessionHandler &other);
		~SessionHandler();

		static std::string getCookieHeader(const Request &request);
		static std::string normalizePath(const std::string &path);
		static Response handleSession(const Request &request);
		static Response handleLogout(const Request &request);
		static std::string buildSessionPage(const SessionData &session, bool created);
		static std::string buildLogoutPage(bool destroyed);
};

#endif
