#include "CgiManager.hpp"
#include "CgiCompletionHandler.hpp"
#include "CgiEventHandler.hpp"
#include "CgiStartupHandler.hpp"
#include "CgiTimeoutHandler.hpp"

CgiManager::CgiManager() {}

CgiManager::CgiManager(const CgiManager &other)
{
	*this = other;
}

CgiManager::~CgiManager() {}

CgiManager &CgiManager::operator=(const CgiManager &other)
{
	if (this != &other)
		fdRegistry = other.fdRegistry;
	return (*this);
}

bool CgiManager::isCgiFd(int fd) const
{
	return (fdRegistry.contains(fd));
}

void CgiManager::cleanup(Client &client, PollManager &pollManager)
{
	CgiCompletionHandler::cleanup(client, pollManager, fdRegistry);
}

int CgiManager::getPollTimeoutMs(const std::map<int, Client> &clients) const
{
	return (CgiTimeoutHandler::getPollTimeoutMs(clients));
}

int CgiManager::checkTimeouts(std::map<int, Client> &clients, const std::vector<ServerConfig> &configs, PollManager &pollManager)
{
	return (CgiTimeoutHandler::handleTimeouts(clients, configs, pollManager, fdRegistry));
}

int CgiManager::handleEvent(int cgiFd, short revents, std::map<int, Client> &clients, const std::vector<ServerConfig> &configs, PollManager &pollManager)
{
	return (CgiEventHandler::handleEvent(cgiFd, revents, clients, configs, pollManager, fdRegistry));
}

int CgiManager::startForClient(Client &client, const RouteConfig &route, const ServerConfig &server, PollManager &pollManager)
{
	return (CgiStartupHandler::startForClient(client, route, server, pollManager, fdRegistry));
}
