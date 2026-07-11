#include "CgiManager.hpp"
#include "CgiCompletionHandler.hpp"
#include "CgiEventHandler.hpp"
#include "CgiStartupHandler.hpp"
#include "CgiTimeoutHandler.hpp"

/**
 * @brief Construct a CgiManager.
 */
CgiManager::CgiManager() {}

/**
 * @brief Copy constructor for CgiManager.
 * @param other - manager to copy
 */
CgiManager::CgiManager(const CgiManager &other)
{
	*this = other;
}

/**
 * @brief Destroy the CgiManager.
 */
CgiManager::~CgiManager() {}

/**
 * @brief Assignment operator.
 * @param other - manager to assign from
 * @return reference to this
 */
CgiManager &CgiManager::operator=(const CgiManager &other)
{
	if (this != &other)
		fdRegistry = other.fdRegistry;
	return (*this);
}

/**
 * @brief Check if a file descriptor belongs to an active CGI session.
 * @param fd - file descriptor to check
 * @return true if it is a CGI fd
 */
bool CgiManager::isCgiFd(int fd) const
{
	return (fdRegistry.contains(fd));
}

/**
 * @brief Cleanup CGI resources for a client via completion handler.
 * @param client - client to cleanup
 * @param pollManager - poll manager to update
 */
void CgiManager::cleanup(Client &client, PollManager &pollManager)
{
	CgiCompletionHandler::cleanup(client, pollManager, fdRegistry);
}

/**
 * @brief Get next poll timeout in milliseconds for CGI handling.
 * @param clients - map of clients to consider
 * @return timeout in ms or -1 for no timeout
 */
int CgiManager::getPollTimeoutMs(const std::map<int, Client> &clients) const
{
	return (CgiTimeoutHandler::getPollTimeoutMs(clients));
}

/**
 * @brief Check and handle CGI timeouts for all clients.
 * @param clients - clients map to check
 * @param configs - server configurations
 * @param pollManager - poll manager to update
 * @return 0 on success
 */
int CgiManager::checkTimeouts(std::map<int, Client> &clients, const std::vector<ServerConfig> &configs, PollManager &pollManager)
{
	return (CgiTimeoutHandler::handleTimeouts(clients, configs, pollManager, fdRegistry));
}

/**
 * @brief Forward a CGI fd event to the event handler.
 * @param cgiFd - CGI file descriptor
 * @param revents - poll events flags
 * @param clients - map of clients
 * @param configs - server configurations
 * @param pollManager - poll manager for events
 * @return handler result
 */
int CgiManager::handleEvent(int cgiFd, short revents, std::map<int, Client> &clients, const std::vector<ServerConfig> &configs, PollManager &pollManager)
{
	return (CgiEventHandler::handleEvent(cgiFd, revents, clients, configs, pollManager, fdRegistry));
}

/**
 * @brief Start a CGI process for a client using the startup handler.
 * @param client - client requesting CGI
 * @param route - route configuration for the request
 * @param server - server configuration
 * @param pollManager - poll manager to register fds
 * @return 0 on success, non-zero on failure
 */
int CgiManager::startForClient(Client &client, const RouteConfig &route, const ServerConfig &server, PollManager &pollManager)
{
	return (CgiStartupHandler::startForClient(client, route, server, pollManager, fdRegistry));
}
