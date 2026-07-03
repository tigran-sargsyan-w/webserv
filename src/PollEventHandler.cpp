#include "PollEventHandler.hpp"
#include "ClientEventHandler.hpp"
#include <iostream>
#include <unistd.h>

static void closeAndRemoveFd(int fd, std::map<int, Client> &clients,
	CgiManager &cgiManager, ListenerSocketHandler &listenerSocketHandler,
	PollManager &pollManager)
{
	std::map<int, Client>::iterator clientIt;

	clientIt = clients.find(fd);
	if (clientIt != clients.end())
	{
		if (clientIt->second.cgi.isActive())
		{
			std::cout << "Cleaning CGI for disconnected client fd "
				<< fd << std::endl;
			cgiManager.cleanup(clientIt->second, pollManager);
		}
		clients.erase(clientIt);
	}
	close(fd);
	pollManager.removeFd(fd);
	listenerSocketHandler.removeFd(fd);
}

namespace
{
	bool shouldCloseClient(ClientEventHandler::Result result)
	{
		return (result == ClientEventHandler::CLIENT_SHOULD_CLOSE
			|| result == ClientEventHandler::EVENT_FAILED);
	}

	PollEventHandler::Result handleCgiFd(const pollfd &pollFd,
		std::map<int, Client> &clients,
		const std::vector<ServerConfig> &configs,
		CgiManager &cgiManager, PollManager &pollManager)
	{
		if (cgiManager.handleEvent(pollFd.fd, pollFd.revents, clients,
				configs, pollManager) == 0)
			return (PollEventHandler::ADVANCE_INDEX);
		return (PollEventHandler::KEEP_INDEX);
	}

	PollEventHandler::Result handleListenerFd(const pollfd &pollFd,
		std::map<int, Client> &clients,
		ListenerSocketHandler &listenerSocketHandler, PollManager &pollManager)
	{
		if (pollFd.revents & POLLIN)
			listenerSocketHandler.acceptConnection(pollFd.fd, clients, pollManager);
		return (PollEventHandler::ADVANCE_INDEX);
	}

	PollEventHandler::Result handleClientFd(const pollfd &pollFd,
		std::map<int, Client> &clients,
		const std::vector<ServerConfig> &configs, CgiManager &cgiManager,
		ListenerSocketHandler &listenerSocketHandler, PollManager &pollManager)
	{
		std::map<int, Client>::iterator	clientIt;
		ClientEventHandler::Result		result;

		clientIt = clients.find(pollFd.fd);
		if (clientIt == clients.end())
			return (PollEventHandler::ADVANCE_INDEX);
		result = ClientEventHandler::handle(clientIt->second, pollFd.revents,
			configs[clientIt->second.serverIndex], cgiManager, pollManager);
		if (shouldCloseClient(result))
		{
			closeAndRemoveFd(pollFd.fd, clients, cgiManager,
				listenerSocketHandler, pollManager);
			return (PollEventHandler::KEEP_INDEX);
		}
		return (PollEventHandler::ADVANCE_INDEX);
	}
}

PollEventHandler::Result PollEventHandler::handle(const pollfd &pollFd,
	std::map<int, Client> &clients,
	const std::vector<ServerConfig> &configs, CgiManager &cgiManager,
	ListenerSocketHandler &listenerSocketHandler, PollManager &pollManager)
{
	if (cgiManager.isCgiFd(pollFd.fd))
		return (handleCgiFd(pollFd, clients, configs, cgiManager, pollManager));
	if (pollFd.revents & (POLLERR | POLLHUP | POLLNVAL))
	{
		closeAndRemoveFd(pollFd.fd, clients, cgiManager,
			listenerSocketHandler, pollManager);
		return (KEEP_INDEX);
	}
	if (listenerSocketHandler.isListeningFd(pollFd.fd))
		return (handleListenerFd(pollFd, clients,
			listenerSocketHandler, pollManager));
	return (handleClientFd(pollFd, clients, configs, cgiManager,
		listenerSocketHandler, pollManager));
}

void PollEventHandler::disconnectClient(int fd,
	std::map<int, Client> &clients,
	CgiManager &cgiManager,
	ListenerSocketHandler &listenerSocketHandler,
	PollManager &pollManager)
{
	closeAndRemoveFd(fd, clients, cgiManager, listenerSocketHandler, pollManager);
}
