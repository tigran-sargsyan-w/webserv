#include "CgiManager.hpp"
#include "CgiCompletionHandler.hpp"
#include "CgiRequestHandler.hpp"
#include "CgiPipeIO.hpp"
#include "CgiValidator.hpp"
#include "CgiHandler.hpp"

#include <fcntl.h>
#include <ctime>
#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <cerrno>
#include <sys/wait.h>
#include <cstring>

CgiManager::CgiManager() {}

CgiManager::CgiManager(const CgiManager &other)
{
	*this = other;
}

CgiManager::~CgiManager() {}

CgiManager &CgiManager::operator=(const CgiManager &other)
{
	if (this != &other)
		fdRegistry = other.fdRegistry;
	return (*this);
}

static int setNonBlockingFd(int fd)
{
	int flags;

	flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
	{
		std::cerr << "fcntl: " << strerror(errno) << "\n";
		return (1);
	}
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
	{
		std::cerr << "fcntl: " << strerror(errno) << "\n";
		return (1);
	}
	return (0);
}

bool CgiManager::isCgiFd(int fd) const
{
	return (fdRegistry.contains(fd));
}

void CgiManager::cleanup(Client &client, PollManager &pollManager)
{
	CgiCompletionHandler::cleanup(client, pollManager, fdRegistry);
}

int CgiManager::writeToCgi(Client &client, PollManager &pollManager)
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

int CgiManager::readFromCgi(Client &client, PollManager &pollManager)
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

int CgiManager::checkFinished(Client &client)
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

// No active CGI          → return -1
// CGI already expired    → return 0
// CGI available, N left  → return N * 1000
int CgiManager::getPollTimeoutMs(const std::map<int, Client> &clients) const
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

int CgiManager::checkTimeouts(std::map<int, Client> &clients, const std::vector<ServerConfig> &configs, PollManager &pollManager)
{
	std::map<int, Client>::iterator it;
	time_t now;

	now = std::time(NULL);
	it = clients.begin();
	while (it != clients.end())
	{
		Client &client = it->second;

		if (client.cgi.pid > 0 && client.cgi.startTime > 0)
		{
			if (now - client.cgi.startTime >= CGI_TIMEOUT_SECONDS)
			{
				std::cout << "CGI timeout for client fd " << client.fd << std::endl;
				CgiCompletionHandler::fail(client, 504, "Gateway Timeout",
					configs, pollManager, fdRegistry);
			}
		}
		++it;
	}
	return (0);
}

int CgiManager::handleEvent(int cgiFd, short revents, std::map<int, Client> &clients, const std::vector<ServerConfig> &configs, PollManager &pollManager)
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

	if (revents & (POLLERR | POLLHUP | POLLNVAL))
	{
		if (cgiFd == client.cgi.stdinFd)
		{
			fdRegistry.closeFd(client.cgi.stdinFd, pollManager);
			client.cgi.stdinFd = -1;
			client.cgi.stdinClosed = true;
			fdRemoved = 1;
		}
		else if (cgiFd == client.cgi.stdoutFd)
		{
			fdRegistry.closeFd(client.cgi.stdoutFd, pollManager);
			client.cgi.stdoutFd = -1;
			client.cgi.stdoutClosed = true;
			fdRemoved = 1;
		}
	}

	if ((revents & POLLOUT) && cgiFd == client.cgi.stdinFd)
	{
		if (writeToCgi(client, pollManager) != 0)
		{
			CgiCompletionHandler::fail(client, 502, "Bad Gateway",
				configs, pollManager, fdRegistry);
			return (1);
		}
		else if (client.cgi.stdinFd == -1)
			fdRemoved = 1;
	}

	if ((revents & POLLIN) && cgiFd == client.cgi.stdoutFd)
	{
		if (readFromCgi(client, pollManager) != 0)
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

int CgiManager::startForClient(Client &client, const RouteConfig &route, const ServerConfig &server, PollManager &pollManager)
{
	CgiContext context;
	CgiProcess process;
	int validationStatus;

	context = CgiRequestHandler::buildContext(client.request, route, server, client.getRemoteAddr());

	validationStatus = CgiValidator::validate(context);
	if (validationStatus != 0)
	{
		CgiCompletionHandler::error(client, validationStatus,
			CgiValidator::messageForStatus(validationStatus), server, pollManager);
		return (0);
	}

	if (CgiHandler::startCgi(context, process) != 0)
	{
		CgiCompletionHandler::error(client, 502, "Bad Gateway",
			server, pollManager);
		return (1);
	}

	if (setNonBlockingFd(process.stdinFd) || setNonBlockingFd(process.stdoutFd))
	{
		close(process.stdinFd);
		close(process.stdoutFd);
		kill(process.pid, SIGKILL);
		waitpid(process.pid, NULL, 0);
		CgiCompletionHandler::error(client, 500, "Internal Server Error",
			server, pollManager);
		return (1);
	}

	client.cgi.pid = process.pid;
	client.cgi.stdinFd = process.stdinFd;
	client.cgi.stdoutFd = process.stdoutFd;
	client.cgi.inputBuffer = context.requestBody;
	client.cgi.inputSent = 0;
	client.cgi.outputBuffer.clear();
	client.cgi.stdinClosed = false;
	client.cgi.stdoutClosed = false;
	client.cgi.finished = false;
	client.cgi.startTime = std::time(NULL);

	fdRegistry.registerFd(client.cgi.stdoutFd, client.fd);
	pollManager.addFd(client.cgi.stdoutFd, POLLIN);

	if (client.cgi.inputBuffer.empty())
	{
		close(client.cgi.stdinFd);
		client.cgi.stdinFd = -1;
		client.cgi.stdinClosed = true;
		client.state = CGI_READING;
	}
	else
	{
		fdRegistry.registerFd(client.cgi.stdinFd, client.fd);
		pollManager.addFd(client.cgi.stdinFd, POLLOUT);
		client.state = CGI_WRITING;
	}

	pollManager.setEvents(client.fd, POLLIN);
	return (0);
}
