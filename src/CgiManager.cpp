#include "CgiManager.hpp"
#include "CgiCompletionHandler.hpp"
#include "CgiPipeIO.hpp"
#include "CgiStartupHandler.hpp"
#include "CgiTimeoutHandler.hpp"

#include <sys/wait.h>

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

int CgiManager::writeToCgi(Client &client, PollManager &pollManager)
{
	if (client.cgi.stdinFd == -1 || client.cgi.stdinClosed)
		return (0);
	if (CgiPipeIO::hasInputFinished(client.cgi))
	{
		fdRegistry.closeFd(client.cgi.stdinFd, pollManager);
		client.cgi.stdinFd = -1;
		client.cgi.stdinClosed = true;
		return (0);
	}
	if (CgiPipeIO::writeToStdin(client.cgi) != 0)
		return (1);
	if (CgiPipeIO::hasInputFinished(client.cgi))
	{
		fdRegistry.closeFd(client.cgi.stdinFd, pollManager);
		client.cgi.stdinFd = -1;
		client.cgi.stdinClosed = true;
	}
	return (0);
}

int CgiManager::readFromCgi(Client &client, PollManager &pollManager)
{
	CgiPipeIO::ReadResult result;

	if (client.cgi.stdoutFd == -1 || client.cgi.stdoutClosed)
		return (0);
	result = CgiPipeIO::readFromStdout(client.cgi);
	if (result == CgiPipeIO::READ_ERROR)
		return (1);
	if (result == CgiPipeIO::READ_EOF)
	{
		fdRegistry.closeFd(client.cgi.stdoutFd, pollManager);
		client.cgi.stdoutFd = -1;
		client.cgi.stdoutClosed = true;
	}
	return (0);
}

int CgiManager::checkFinished(Client &client)
{
	int status;
	pid_t result;

	if (client.cgi.pid <= 0)
		return (0);
	result = waitpid(client.cgi.pid, &status, WNOHANG);
	if (result == 0)
		return (0);
	if (result != client.cgi.pid)
		return (1);
	client.cgi.pid = -1;
	client.cgi.finished = true;
	if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
		return (0);
	return (1);
}

int CgiManager::getPollTimeoutMs(const std::map<int, Client> &clients) const
{
	return (CgiTimeoutHandler::getPollTimeoutMs(clients));
}

int CgiManager::checkTimeouts(std::map<int, Client> &clients,
	const std::vector<ServerConfig> &configs, PollManager &pollManager)
{
	return (CgiTimeoutHandler::handleTimeouts(clients, configs,
		pollManager, fdRegistry));
}

int CgiManager::handleEvent(int cgiFd, short revents,
	std::map<int, Client> &clients,
	const std::vector<ServerConfig> &configs, PollManager &pollManager)
{
	std::map<int, Client>::iterator clientIt;
	int fdRemoved;
	int clientFd;

	fdRemoved = 0;
	if (!fdRegistry.getClientFd(cgiFd, clientFd))
		return (1);
	clientIt = clients.find(clientFd);
	if (clientIt == clients.end())
	{
		fdRegistry.closeFd(cgiFd, pollManager);
		return (1);
	}

	Client &client = clientIt->second;
	if (revents & (POLLERR | POLLHUP | POLLNVAL))
	{
		if (cgiFd == client.cgi.stdinFd)
		{
			fdRegistry.closeFd(client.cgi.stdinFd, pollManager);
			client.cgi.stdinFd = -1;
			client.cgi.stdinClosed = true;
			fdRemoved = 1;
		}
		else if (cgiFd == client.cgi.stdoutFd)
		{
			fdRegistry.closeFd(client.cgi.stdoutFd, pollManager);
			client.cgi.stdoutFd = -1;
			client.cgi.stdoutClosed = true;
			fdRemoved = 1;
		}
	}
	if ((revents & POLLOUT) && cgiFd == client.cgi.stdinFd)
	{
		if (writeToCgi(client, pollManager) != 0)
		{
			CgiCompletionHandler::fail(client, 502, "Bad Gateway",
				configs, pollManager, fdRegistry);
			return (1);
		}
		else if (client.cgi.stdinFd == -1)
			fdRemoved = 1;
	}
	if ((revents & POLLIN) && cgiFd == client.cgi.stdoutFd)
	{
		if (readFromCgi(client, pollManager) != 0)
		{
			CgiCompletionHandler::fail(client, 502, "Bad Gateway",
				configs, pollManager, fdRegistry);
			return (1);
		}
		else if (client.cgi.stdoutFd == -1)
			fdRemoved = 1;
	}
	if (checkFinished(client) != 0)
	{
		CgiCompletionHandler::fail(client, 502, "Bad Gateway",
			configs, pollManager, fdRegistry);
		return (1);
	}
	if (client.cgi.stdoutClosed && client.cgi.finished)
		CgiCompletionHandler::finish(client, pollManager);
	return (fdRemoved);
}

int CgiManager::startForClient(Client &client, const RouteConfig &route,
	const ServerConfig &server, PollManager &pollManager)
{
	return (CgiStartupHandler::startForClient(client, route, server,
		pollManager, fdRegistry));
}
