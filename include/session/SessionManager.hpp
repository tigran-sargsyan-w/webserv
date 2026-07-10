#ifndef SESSIONMANAGER_HPP
#define SESSIONMANAGER_HPP

#include <ctime>
#include <map>
#include <string>

struct SessionData
{
	std::string id;
	std::time_t createdAt;
	std::time_t lastSeen;
	unsigned long visitCount;
};

class SessionManager
{
	public:
		static SessionData getOrCreate(const std::string &cookieHeader, bool &created);
		static bool destroy(const std::string &cookieHeader);
		static std::string buildCookieHeader(const std::string &sessionId);
		static std::string buildExpiredCookieHeader();

	private:
		SessionManager();
		SessionManager(const SessionManager &other);
		SessionManager &operator=(const SessionManager &other);
		~SessionManager();

		static std::map<std::string, SessionData> &sessions();
		static unsigned long &counter();
		static std::string generateId();
		static bool isValidId(const std::string &sessionId);
		static void cleanupExpired();
};

#endif
