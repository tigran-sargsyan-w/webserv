#include "CgiTimeoutHandler.hpp"
#include "CgiCompletionHandler.hpp"

#include <ctime>
#include <iostream>

int	CgiTimeoutHandler::getPollTimeoutMs(const std::map<int, Client> &clients)
{
	std::map<int, Client>::const_iterator it;
	time_t now;
	int shortestTimeout;
	int elapsed;
	int remaining;

	shortestTimeout = -1;
	now = std::time(NULL);

	it = clients.begin();
	while (it != clients.end())
	{
		const Client &client = it->second;

		if (client.cgi.pid > 0 && client.cgi.startTime > 0)
		{
			elapsed = static_cast<int>(now - client.cgi.startTime);
			remaining = CGI_TIMEOUT_SECONDS - elapsed;

			if (remaining <= 0)
				return (0);

			if (shortestTimeout == -1 || remaining * 1000 < shortestTimeout)
				shortestTimeout = remaining * 1000;
		}
		++it;
	}
	return (shortestTimeout);
}

int	CgiTimeoutHandler::handleTimeouts(std::map<int, Client> &clients,
	const std::vector<ServerConfig> &configs, PollManager &pollManager,
	CgiFdRegistry &fdRegistry)
{
	std::map<int, Client>::iterator it;
	time_t now;

	now = std::time(NULL);
	it = clients.begin();
	while (it != clients.end())
	{
		Client &client = it->second;

		if (isExpired(client, now))
		{
			std::cout << "CGI timeout for client fd " << client.fd << std::endl;
			CgiCompletionHandler::fail(client, 504, "Gateway Timeout",
				configs, pollManager, fdRegistry);
		}
		++it;
	}
	return (0);
}

bool	CgiTimeoutHandler::isExpired(const Client &client, time_t now)
{
	return (client.cgi.pid > 0 && client.cgi.startTime > 0
		&& now - client.cgi.startTime >= CGI_TIMEOUT_SECONDS);
}
