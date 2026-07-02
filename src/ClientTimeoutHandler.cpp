#include "ClientTimeoutHandler.hpp"

#include <iostream>

int	ClientTimeoutHandler::getPollTimeoutMs(const std::map<int, Client> &clients,
	int timeoutSeconds)
{
	std::map<int, Client>::const_iterator	it;
	time_t					now;
	int					shortestTimeout;
	int					elapsed;
	int					remaining;
	shortestTimeout = -1;
	now = std::time(NULL);
	it = clients.begin();
	while (it != clients.end())
	{
		const Client	&client = it->second;
		if (!client.cgi.isActive())
		{
			elapsed = static_cast<int>(now - client.lastActivity);
			remaining = timeoutSeconds - elapsed;
			if (remaining <= 0)
				return (0);
			if (shortestTimeout == -1 || remaining * 1000 < shortestTimeout)
				shortestTimeout = remaining * 1000;
		}
		++it;
	}
	return (shortestTimeout);
}

void	ClientTimeoutHandler::collectExpiredClients(
	const std::map<int, Client> &clients, int timeoutSeconds,
	std::vector<int> &expiredFds)
{
	std::map<int, Client>::const_iterator	it;
	time_t					now;
	now = std::time(NULL);
	it = clients.begin();
	while (it != clients.end())
	{
		const Client	&client = it->second;
		if (isExpired(client, now, timeoutSeconds))
		{
			std::cout << "Client timeout for fd " << client.fd
				<< " after " << timeoutSeconds << "s of inactivity"
				<< std::endl;
			expiredFds.push_back(client.fd);
		}
		++it;
	}
}

bool	ClientTimeoutHandler::isExpired(const Client &client, time_t now,
	int timeoutSeconds)
{
	if (client.cgi.isActive())
		return (false);
	return (now - client.lastActivity >= timeoutSeconds);
}