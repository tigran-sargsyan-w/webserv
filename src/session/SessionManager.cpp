#include "SessionManager.hpp"

#include "CookieParser.hpp"

#include <cctype>
#include <sstream>
#include <unistd.h>

#define SESSION_TTL_SECONDS 1800

/**
 * @brief Gets the static session storage map
 * @return reference to session storage
 */
std::map<std::string, SessionData> &SessionManager::sessions()
{
	static std::map<std::string, SessionData> storage;

	return (storage);
}

/**
 * @brief Gets the static session counter for ID generation
 * @return reference to session counter
 */
unsigned long &SessionManager::counter()
{
	static unsigned long value = 0;

	return (value);
}

/**
 * @brief Validates session ID format and length
 * @param sessionId - session ID to validate
 * @return true if session ID is valid
 */
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

/**
 * @brief Generates a unique session ID
 * @return new session ID
 */
std::string SessionManager::generateId()
{
	std::ostringstream oss;

	++counter();
	oss << "ws_" << std::time(NULL) << "_" << getpid() << "_" << counter();
	return (oss.str());
}

/**
 * @brief Removes expired sessions from storage
 */
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

/**
 * @brief Gets existing session or creates a new one
 * @param cookieHeader - raw Cookie header value
 * @param created - output parameter, true if new session was created
 * @return session data
 */
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

/**
 * @brief Destroys a session by ID from cookie header
 * @param cookieHeader - raw Cookie header value
 * @return true if session was found and destroyed
 */
bool SessionManager::destroy(const std::string &cookieHeader)
{
	std::map<std::string, SessionData> &storage = sessions();
	std::string sessionId = CookieParser::getCookieValue(cookieHeader, "sid");

	cleanupExpired();
	if (!isValidId(sessionId))
		return (false);
	return (storage.erase(sessionId) > 0);
}

/**
 * @brief Builds Set-Cookie header for a session
 * @param sessionId - session ID to set
 * @return Set-Cookie header value
 */
std::string SessionManager::buildCookieHeader(const std::string &sessionId)
{
	return ("sid=" + sessionId + "; Path=/; Max-Age=1800; HttpOnly; SameSite=Lax");
}

/**
 * @brief Builds Set-Cookie header to expire session cookie
 * @return Set-Cookie header value for cookie expiration
 */
std::string SessionManager::buildExpiredCookieHeader()
{
	return ("sid=deleted; Path=/; Max-Age=0; HttpOnly; SameSite=Lax");
}
