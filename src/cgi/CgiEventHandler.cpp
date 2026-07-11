#include "CgiEventHandler.hpp"
#include "CgiCompletionHandler.hpp"
#include "CgiPipeIO.hpp"

#include <sys/wait.h>

namespace
{
	/**
	 * @brief Close CGI stdin fd and mark it closed in the session.
	 * @param client - client owning the CGI session
	 * @param pollManager - poll manager to update
	 * @param fdRegistry - registry of CGI fds
	 */
	void	closeCgiStdin(Client &client, PollManager &pollManager,
		CgiFdRegistry &fdRegistry)
	{
		fdRegistry.closeFd(client.cgi.stdinFd, pollManager);
		client.cgi.stdinFd = -1;
		client.cgi.stdinClosed = true;
	}

	/**
	 * @brief Close CGI stdout fd and mark it closed in the session.
	 * @param client - client owning the CGI session
	 * @param pollManager - poll manager to update
	 * @param fdRegistry - registry of CGI fds
	 */
	void	closeCgiStdout(Client &client, PollManager &pollManager,
		CgiFdRegistry &fdRegistry)
	{
		fdRegistry.closeFd(client.cgi.stdoutFd, pollManager);
		client.cgi.stdoutFd = -1;
		client.cgi.stdoutClosed = true;
	}

	/**
	 * @brief Handle errors on a CGI fd by closing the matching fd.
	 * @param cgiFd - file descriptor that reported an error
	 * @param client - client owning the CGI session
	 * @param pollManager - poll manager to update
	 * @param fdRegistry - registry of CGI fds
	 * @return 1 if handled, 0 otherwise
	 */
	int	handleCgiFdError(int cgiFd, Client &client,
		PollManager &pollManager, CgiFdRegistry &fdRegistry)
	{
		if (cgiFd == client.cgi.stdinFd)
		{
			closeCgiStdin(client, pollManager, fdRegistry);
			return (1);
		}
		if (cgiFd == client.cgi.stdoutFd)
		{
			closeCgiStdout(client, pollManager, fdRegistry);
			return (1);
		}
		return (0);
	}
}

/**
 * @brief Dispatch and handle events for a CGI-related fd.
 * @param cgiFd - the CGI file descriptor that has events
 * @param revents - poll events flags
 * @param clients - map of client fds to clients
 * @param configs - server configurations vector
 * @param pollManager - poll manager used for fd events
 * @param fdRegistry - registry mapping CGI fds to client fds
 * @return 1 on error/remove, 0 otherwise
 */
int	CgiEventHandler::handleEvent(int cgiFd, short revents,
	std::map<int, Client> &clients,
	const std::vector<ServerConfig> &configs, PollManager &pollManager,
	CgiFdRegistry &fdRegistry)
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
	if (revents & (POLLERR | POLLNVAL))
		fdRemoved = handleCgiFdError(cgiFd, client, pollManager, fdRegistry);
	if ((revents & POLLHUP) && cgiFd == client.cgi.stdinFd)
	{
		closeCgiStdin(client, pollManager, fdRegistry);
		fdRemoved = 1;
	}
	if ((revents & POLLOUT) && cgiFd == client.cgi.stdinFd)
	{
		if (writeToCgi(client, pollManager, fdRegistry) != 0)
		{
			CgiCompletionHandler::fail(client, 502, "Bad Gateway",
				configs, pollManager, fdRegistry);
			return (1);
		}
		else if (client.cgi.stdinFd == -1)
			fdRemoved = 1;
	}
	if ((revents & (POLLIN | POLLHUP)) && cgiFd == client.cgi.stdoutFd)
	{
		if (readFromCgi(client, pollManager, fdRegistry) != 0)
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

/**
 * @brief Write pending request body data to the CGI stdin.
 * @param client - client containing the CGI session and buffers
 * @param pollManager - poll manager to update fds
 * @param fdRegistry - registry of CGI fds
 * @return 0 on success, 1 on write error
 */
int	CgiEventHandler::writeToCgi(Client &client, PollManager &pollManager,
	CgiFdRegistry &fdRegistry)
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

/**
 * @brief Read available data from CGI stdout into the session buffer.
 * @param client - client containing the CGI session
 * @param pollManager - poll manager to update fds
 * @param fdRegistry - registry of CGI fds
 * @return 0 on success, 1 on read error
 */
int	CgiEventHandler::readFromCgi(Client &client, PollManager &pollManager,
	CgiFdRegistry &fdRegistry)
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

/**
 * @brief Check whether the CGI child has exited and update session.
 * @param client - client containing the CGI session
 * @return 0 if not finished or exited successfully, 1 on error/abnormal exit
 */
int	CgiEventHandler::checkFinished(Client &client)
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
