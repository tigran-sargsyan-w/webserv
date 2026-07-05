#include "SessionManager.hpp"

#include "CookieParser.hpp"

#include <cctype>
#include <sstream>
#include <unistd.h>

#define SESSION_TTL_SECONDS 1800

std::map<std::string, SessionData> &SessionManager::sessions()
{
	static std::map<std::string, SessionData> storage;

	return (storage);
}

unsigned long &SessionManager::counter()
{
	static unsigned long value = 0;

	return (value);
}

bool SessionManager::isValidId(const std::string &sessionId)
{
	if (sessionId.empty() || sessionId.size() > 128)
		return (false);
	for (size_t i = 0; i < sessionId.size(); ++i)
	{
		unsigned char c = static_cast<unsigned char>(sessionId[i]);

		if (!std::isalnum(c) && c != '_' && c != '-')
			return (false);
	}
	return (true);
}

std::string SessionManager::generateId()
{
	std::ostringstream oss;

	++counter();
	oss << "ws_" << std::time(NULL) << "_" << getpid() << "_" << counter();
	return (oss.str());
}

void SessionManager::cleanupExpired()
{
	std::map<std::string, SessionData> &storage = sessions();
	std::map<std::string, SessionData>::iterator it = storage.begin();
	std::time_t now = std::time(NULL);

	while (it != storage.end())
	{
		if (now - it->second.lastSeen > SESSION_TTL_SECONDS)
			storage.erase(it++);
		else
			++it;
	}
}

SessionData SessionManager::getOrCreate(const std::string &cookieHeader, bool &created)
{
	std::map<std::string, SessionData> &storage = sessions();
	std::string sessionId = CookieParser::getCookieValue(cookieHeader, "sid");
	std::map<std::string, SessionData>::iterator it;
	std::time_t now = std::time(NULL);

	cleanupExpired();
	created = false;
	if (isValidId(sessionId))
	{
		it = storage.find(sessionId);
		if (it != storage.end())
		{
			it->second.lastSeen = now;
			++it->second.visitCount;
			return (it->second);
		}
	}
	sessionId = generateId();
	SessionData session;
	session.id = sessionId;
	session.createdAt = now;
	session.lastSeen = now;
	session.visitCount = 1;
	storage[sessionId] = session;
	created = true;
	return (session);
}

bool SessionManager::destroy(const std::string &cookieHeader)
{
	std::map<std::string, SessionData> &storage = sessions();
	std::string sessionId = CookieParser::getCookieValue(cookieHeader, "sid");

	cleanupExpired();
	if (!isValidId(sessionId))
		return (false);
	return (storage.erase(sessionId) > 0);
}

std::string SessionManager::buildCookieHeader(const std::string &sessionId)
{
	return ("sid=" + sessionId + "; Path=/; Max-Age=1800; HttpOnly; SameSite=Lax");
}

std::string SessionManager::buildExpiredCookieHeader()
{
	return ("sid=deleted; Path=/; Max-Age=0; HttpOnly; SameSite=Lax");
}
