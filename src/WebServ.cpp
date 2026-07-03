#include "WebServ.hpp"
#include "PollEventHandler.hpp"
#include <cerrno>
#include <cstring>
#include <iostream>
#include <poll.h>
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

static int combinePollTimeoutMs(int first, int second)
{
	if (first == 0 || second == 0)
		return (0);
	if (first < 0)
		return (second);
	if (second < 0)
		return (first);
	if (first < second)
		return (first);
	return (second);
}

int WebServ::run()
{
	std::cout << "WebServ run called!\n";

	while (true)
	{
		std::vector<pollfd> &pollFds = pollManager.getFds();

		if (pollManager.empty())
			return (1);

		int cgiPollTimeout;
		int clientPollTimeout;
		int pollTimeout;
		int ready;

		cgiPollTimeout = cgiManager.getPollTimeoutMs(connectionManager.getClients());
		clientPollTimeout = connectionManager.getPollTimeoutMs(configs);
		pollTimeout = combinePollTimeoutMs(cgiPollTimeout, clientPollTimeout);
		ready = poll(&pollFds[0], pollFds.size(), pollTimeout);

		if (ready < 0)
		{
			if (errno == EINTR)
				continue;
			std::cerr << "poll: " << strerror(errno) << std::endl;
			return (1);
		}
		cgiManager.checkTimeouts(connectionManager.getClients(), configs, pollManager);
		connectionManager.enforceTimeouts(configs, cgiManager, listenerSocketHandler, pollManager);
		if (ready == 0)
			continue;
		std::cout << "Sockets Ready - " << ready << "\n"
				  << std::endl;
		size_t i = 0;
		while (i < pollFds.size())
		{
			if (PollEventHandler::handle(pollFds[i], connectionManager.getClients(), configs,
										 cgiManager, listenerSocketHandler, pollManager) == PollEventHandler::ADVANCE_INDEX)
				++i;
		}
	}
}
