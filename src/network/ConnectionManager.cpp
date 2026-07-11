#include "ConnectionManager.hpp"
#include "ClientTimeoutHandler.hpp"
#include "PollEventHandler.hpp"

/**
 * @brief Create an empty connection manager.
 */
ConnectionManager::ConnectionManager() {}

/**
 * @brief Get the mutable client map.
 * @return tracked clients
 */
std::map<int, Client> &ConnectionManager::getClients()
{
    return (clients);
}

/**
 * @brief Get the immutable client map.
 * @return tracked clients
 */
const std::map<int, Client> &ConnectionManager::getClients() const
{
    return (clients);
}

/**
 * @brief Compute the current poll timeout from active clients.
 * @param configs - server configuration list
 * @return timeout in milliseconds
 */
int ConnectionManager::getPollTimeoutMs(
    const std::vector<ServerConfig> &configs) const
{
    return (ClientTimeoutHandler::getPollTimeoutMs(clients, configs));
}

/**
 * @brief Disconnect clients whose inactivity timeout expired.
 * @param configs - server configuration list
 * @param cgiManager - CGI manager used for cleanup
 * @param listenerSocketHandler - listener registry used for removal
 * @param pollManager - poll manager used to drop sockets
 */
void ConnectionManager::enforceTimeouts(
    const std::vector<ServerConfig> &configs,
    CgiManager &cgiManager, ListenerSocketHandler &listenerSocketHandler,
    PollManager &pollManager)
{
    std::vector<int> expiredFds;
    size_t i;
    ClientTimeoutHandler::collectExpiredClients(clients, configs, expiredFds);
    i = 0;
    while (i < expiredFds.size())
    {
        PollEventHandler::disconnectClient(expiredFds[i], clients,
                                           cgiManager, listenerSocketHandler, pollManager);
        ++i;
    }
}