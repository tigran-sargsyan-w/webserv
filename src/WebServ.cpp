#include "WebServ.hpp"
#include "PollEventHandler.hpp"
#include <cerrno>
#include <cstring>
#include <iostream>
#include <poll.h>

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

int WebServ::run()
{
	std::cout << "WebServ run called!\n";

	while (true)
	{
		std::vector<pollfd> &pollFds = pollManager.getFds();

		if (pollManager.empty())
			return (1);

		int ready = poll(&pollFds[0], pollFds.size(),
			cgiManager.getPollTimeoutMs(clients));

		if (ready < 0)
		{
			if (errno == EINTR)
				continue;
			std::cerr << "poll: " << strerror(errno) << std::endl;
			return (1);
		}
		cgiManager.checkTimeouts(clients, configs, pollManager);
		if (ready == 0)
			continue;
		std::cout << "Sockets Ready - " << ready << "\n" << std::endl;
		size_t i = 0;
		while (i < pollFds.size())
		{
			if (PollEventHandler::handle(pollFds[i], clients, configs,
					cgiManager, listenerSocketHandler, pollManager)
				== PollEventHandler::ADVANCE_INDEX)
				++i;
		}
	}
}
