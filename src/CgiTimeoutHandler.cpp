#include "CgiTimeoutHandler.hpp"
#include "CgiCompletionHandler.hpp"
#include "Logger.hpp"

#include <ctime>
#include <sys/wait.h>

namespace
{
	static const int CGI_REAP_POLL_INTERVAL_MS = 100;

	int	reapFinishedChild(Client &client)
	{
		int		status;
		pid_t	result;

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
}

int	CgiTimeoutHandler::getPollTimeoutMs(const std::map<int, Client> &clients)
{
	std::map<int, Client>::const_iterator it;
	time_t now;
	int shortestTimeout;
	int elapsed;
	int remaining;
	int candidate;

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
			candidate = remaining * 1000;
			if (client.cgi.stdoutClosed
				&& candidate > CGI_REAP_POLL_INTERVAL_MS)
				candidate = CGI_REAP_POLL_INTERVAL_MS;
			if (shortestTimeout == -1 || candidate < shortestTimeout)
				shortestTimeout = candidate;
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

		if (client.cgi.hasActiveProcess() && reapFinishedChild(client) != 0)
		{
			CgiCompletionHandler::fail(client, 502, "Bad Gateway",
				configs, pollManager, fdRegistry);
			++it;
			continue;
		}
		if (client.cgi.stdoutClosed && client.cgi.finished)
		{
			CgiCompletionHandler::finish(client, pollManager);
			++it;
			continue;
		}
		if (isExpired(client, now))
		{
			Logger::info() << "CGI timeout for client fd " << client.fd
				<< std::endl;
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
