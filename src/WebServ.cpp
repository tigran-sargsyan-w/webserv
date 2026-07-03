#include "WebServ.hpp"
#include "ClientEventHandler.hpp"
#include "ClientTimeoutHandler.hpp"
#include <cerrno>
#include <cstring>
#include <iostream>
#include <poll.h>
#include <sys/poll.h>
#include <unistd.h>
#include <vector>

WebServ::WebServ()
{
	std::cout << "WebServ created!\n";
}

WebServ::WebServ(const WebServ &other)
{
	(void)other;
	std::cout << "WebServ copy constructor called!\n";
}

WebServ::~WebServ()
{
	std::cout << "WebServ destroyed!\n";
}

WebServ &WebServ::operator=(const WebServ &other)
{
	(void)other;
	std::cout << "WebServ assignement operator called!\n";
	return (*this);
}

int WebServ::setup(std::vector<ServerConfig> servers)
{
	std::cout << "WebServ setup called!\n";
	configs = servers;
	return (listenerSocketHandler.setup(configs, pollManager));
}

static bool hasActiveCgi(const Client &client)
{
	return (client.cgi.isActive());
}

void WebServ::closeAndRemoveFd(int fd)
{
	std::map<int, Client>::iterator clientIt;

	clientIt = clients.find(fd);
	if (clientIt != clients.end())
	{
		if (hasActiveCgi(clientIt->second))
		{
			std::cout << "Cleaning CGI for disconnected client fd " << fd << std::endl;
			cgiManager.cleanup(clientIt->second, pollManager);
		}
		clients.erase(clientIt);
	}

	close(fd);
	pollManager.removeFd(fd);
	listenerSocketHandler.removeFd(fd);
}

static bool shouldCloseClient(ClientEventHandler::Result result)
{
	return (result == ClientEventHandler::CLIENT_SHOULD_CLOSE
		|| result == ClientEventHandler::EVENT_FAILED);
}

int WebServ::run()
{
	std::cout << "WebServ run called!\n";

	while (true)
	{
		std::vector<pollfd> &pollFds = pollManager.getFds();

		if (pollManager.empty())
			return (1);

		int ready = poll(&pollFds[0], pollFds.size(), cgiManager.getPollTimeoutMs(clients));

		if (ready < 0)
		{
			if (errno == EINTR)
				continue;
			std::cerr << "poll: " << strerror(errno) << std::endl;
			return (1);
		}
		// Check CGI timeouts on each loop iteration
		cgiManager.checkTimeouts(clients, configs, pollManager);

		// No events, continue polling
		if (ready == 0)
			continue;

		std::cout << "Sockets Ready - " << ready << "\n"
				  << std::endl;

		// PollFds loop

		size_t i = 0;
		while (i < pollFds.size())
		{
			int curFD = pollFds[i].fd;

			if (cgiManager.isCgiFd(curFD))
			{
				if (cgiManager.handleEvent(curFD, pollFds[i].revents, clients, configs, pollManager) == 0)
					++i;
				continue;
			}

			if (pollFds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
			{
				closeAndRemoveFd(curFD);
				continue;
			}

			// Accept connections

			if (listenerSocketHandler.isListeningFd(curFD))
			{
				if (pollFds[i].revents & POLLIN)
					listenerSocketHandler.acceptConnection(curFD, clients, pollManager);

				++i;
				continue;
			}
			else
			{
				std::map<int, Client>::iterator clientIt = clients.find(curFD);
				if (clientIt == clients.end())
				{
					++i;
					continue;
				}

				ClientEventHandler::Result result;
				Client &curClient = clientIt->second;

				result = ClientEventHandler::handle(curClient, pollFds[i].revents,
					configs[curClient.serverIndex], cgiManager, pollManager);
				if (shouldCloseClient(result))
				{
					closeAndRemoveFd(curFD);
					continue;
				}
			}
			++i;
		}
	}
}
