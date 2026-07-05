#include "ClientTimeoutHandler.hpp"

#include <iostream>

int ClientTimeoutHandler::getTimeoutSeconds(const Client &client,
											const std::vector<ServerConfig> &configs)
{
	if (client.serverIndex >= configs.size())
		return (30);
	return (configs[client.serverIndex].clientTimeout);
}

int ClientTimeoutHandler::getPollTimeoutMs(const std::map<int, Client> &clients,
										   const std::vector<ServerConfig> &configs)
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
		if (!client.cgi.isActive())
		{
			int timeoutSeconds;

			timeoutSeconds = getTimeoutSeconds(client, configs);
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

void ClientTimeoutHandler::collectExpiredClients(const std::map<int, Client> &clients,
												 const std::vector<ServerConfig> &configs, std::vector<int> &expiredFds)
{
	std::map<int, Client>::const_iterator it;
	time_t now;
	now = std::time(NULL);
	it = clients.begin();
	while (it != clients.end())
	{
		const Client &client = it->second;
		int timeoutSeconds;

		timeoutSeconds = getTimeoutSeconds(client, configs);
		if (isExpired(client, now, timeoutSeconds))
		{
			std::cout << "Client timeout for fd " << client.fd << " after "
					  << timeoutSeconds << "s of inactivity" << std::endl;
			expiredFds.push_back(client.fd);
		}
		++it;
	}
}

bool ClientTimeoutHandler::isExpired(const Client &client, time_t now,
									 int timeoutSeconds)
{
	if (client.cgi.isActive())
		return (false);
	return (now - client.lastActivity >= timeoutSeconds);
}