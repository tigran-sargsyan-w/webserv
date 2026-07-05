#include "ConnectionManager.hpp"
#include "ClientTimeoutHandler.hpp"
#include "PollEventHandler.hpp"

ConnectionManager::ConnectionManager() {}

std::map<int, Client> &ConnectionManager::getClients()
{
    return (clients);
}

const std::map<int, Client> &ConnectionManager::getClients() const
{
    return (clients);
}

int ConnectionManager::getPollTimeoutMs(
    const std::vector<ServerConfig> &configs) const
{
    return (ClientTimeoutHandler::getPollTimeoutMs(clients, configs));
}

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