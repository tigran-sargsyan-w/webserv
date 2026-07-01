#include "CgiTimeout.hpp"

int	CgiTimeout::getPollTimeoutMs(const std::map<int, Client> &clients,
	int timeoutSeconds)
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

bool	CgiTimeout::isExpired(const Client &client, time_t now,
	int timeoutSeconds)
{
	return (client.cgi.pid > 0 && client.cgi.startTime > 0
		&& now - client.cgi.startTime >= timeoutSeconds);
}
