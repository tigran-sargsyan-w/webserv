#include "CgiManager.hpp"
#include "CgiRequestHandler.hpp"
#include "ErrorResponseHandler.hpp"
#include "Response.hpp"

#include <ctime>
#include <iostream>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#include <unistd.h>
#include <sys/wait.h>

CgiManager::CgiManager() {}

CgiManager::CgiManager(const CgiManager &other)
{
	*this = other;
}

CgiManager::~CgiManager() {}

CgiManager &CgiManager::operator=(const CgiManager &other)
{
	if (this != &other)
		cgiFdToClientFd = other.cgiFdToClientFd;
	return (*this);
}

bool CgiManager::isCgiFd(int fd) const
{
	return (cgiFdToClientFd.find(fd) != cgiFdToClientFd.end());
}

void CgiManager::registerCgiFd(int cgiFd, int clientFd)
{
	cgiFdToClientFd[cgiFd] = clientFd;
}

void CgiManager::unregisterCgiFd(int cgiFd)
{
	cgiFdToClientFd.erase(cgiFd);
}

bool CgiManager::getClientFd(int cgiFd, int &clientFd) const
{
	std::map<int, int>::const_iterator it;

	it = cgiFdToClientFd.find(cgiFd);
	if (it == cgiFdToClientFd.end())
		return (false);
	clientFd = it->second;
	return (true);
}

void CgiManager::closeCgiFd(int fd, PollManager &pollManager)
{
	if (fd == -1)
		return;

	close(fd);
	pollManager.removeFd(fd);
	unregisterCgiFd(fd);
}

void CgiManager::resetState(Client &client)
{
	client.cgiPid = -1;
	client.cgiStdinFd = -1;
	client.cgiStdoutFd = -1;
	client.cgiInputBuffer.clear();
	client.cgiInputSent = 0;
	client.cgiOutputBuffer.clear();
	client.cgiStdinClosed = true;
	client.cgiStdoutClosed = true;
	client.cgiFinished = false;
	client.cgiStartTime = 0;
}

void CgiManager::cleanup(Client &client, PollManager &pollManager)
{
	if (client.cgiStdinFd != -1)
		closeCgiFd(client.cgiStdinFd, pollManager);
	if (client.cgiStdoutFd != -1)
		closeCgiFd(client.cgiStdoutFd, pollManager);
	if (client.cgiPid > 0)
	{
		kill(client.cgiPid, SIGKILL);
		waitpid(client.cgiPid, NULL, 0);
	}
	resetState(client);
}

int CgiManager::writeToCgi(Client &client, PollManager &pollManager)
{
	size_t remaining;
	ssize_t bytesWritten;

	if (client.cgiStdinFd == -1 || client.cgiStdinClosed)
		return (0);

	remaining = client.cgiInputBuffer.size() - client.cgiInputSent;
	if (remaining == 0)
	{
		closeCgiFd(client.cgiStdinFd, pollManager);
		client.cgiStdinFd = -1;
		client.cgiStdinClosed = true;
		return (0);
	}

	bytesWritten = write(client.cgiStdinFd, client.cgiInputBuffer.c_str() + client.cgiInputSent, remaining);

	if (bytesWritten == -1)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
			return (0);
		return (1);
	}

	if (bytesWritten == 0)
		return (0);

	client.cgiInputSent += static_cast<size_t>(bytesWritten);

	if (client.cgiInputSent >= client.cgiInputBuffer.size())
	{
		closeCgiFd(client.cgiStdinFd, pollManager);
		client.cgiStdinFd = -1;
		client.cgiStdinClosed = true;
	}

	return (0);
}

int CgiManager::readFromCgi(Client &client, PollManager &pollManager)
{
	char buffer[4096];
	ssize_t bytesRead;

	if (client.cgiStdoutFd == -1 || client.cgiStdoutClosed)
		return (0);

	bytesRead = read(client.cgiStdoutFd, buffer, sizeof(buffer));
	if (bytesRead == -1)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
			return (0);
		return (1);
	}

	if (bytesRead == 0)
	{
		closeCgiFd(client.cgiStdoutFd, pollManager);
		client.cgiStdoutFd = -1;
		client.cgiStdoutClosed = true;
		return (0);
	}

	client.cgiOutputBuffer.append(buffer, bytesRead);
	return (0);
}

int CgiManager::checkFinished(Client &client)
{
	int status;
	pid_t result;

	if (client.cgiPid <= 0)
		return (0);

	result = waitpid(client.cgiPid, &status, WNOHANG);
	if (result == 0)
		return (0);
	if (result != client.cgiPid)
		return (1);

	client.cgiPid = -1;
	client.cgiFinished = true;

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

		if (client.cgiPid > 0 && client.cgiStartTime > 0)
		{
			elapsed = static_cast<int>(now - client.cgiStartTime);
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

		if (client.cgiPid > 0 && client.cgiStartTime > 0)
		{
			if (now - client.cgiStartTime >= CGI_TIMEOUT_SECONDS)
			{
				std::cout << "CGI timeout for client fd " << client.fd << std::endl;
				failResponse(client, 504, "Gateway Timeout", configs, pollManager);
			}
		}
		++it;
	}
	return (0);
}

void CgiManager::finishResponse(Client &client, PollManager &pollManager)
{
	Response response;

	response = CgiRequestHandler::buildResponse(client.cgiOutputBuffer);
	client.responseBuffer = response.toString();
	client.bytesSent = 0;
	client.responseReady = true;
	client.state = WRITING;
	resetState(client);
	pollManager.setEvents(client.fd, POLLOUT);
}

void CgiManager::failResponse(Client &client, int code, const std::string &message, const std::vector<ServerConfig> &configs, PollManager &pollManager)
{
	const ServerConfig &server = configs[client.serverIndex];
	Response response;

	cleanup(client, pollManager);
	response = ErrorResponseHandler::build(code, message, server);
	client.responseBuffer = response.toString();
	client.bytesSent = 0;
	client.responseReady = true;
	client.state = WRITING;
	pollManager.setEvents(client.fd, POLLOUT);
}

int CgiManager::handleEvent(int cgiFd, short revents, std::map<int, Client> &clients, const std::vector<ServerConfig> &configs, PollManager &pollManager)
{
	std::map<int, Client>::iterator clientIt;
	int fdRemoved;
	int clientFd;

	fdRemoved = 0;
	if (!getClientFd(cgiFd, clientFd))
		return (1);
	clientIt = clients.find(clientFd);
	if (clientIt == clients.end())
	{
		close(cgiFd);
		pollManager.removeFd(cgiFd);
		unregisterCgiFd(cgiFd);
		return (1);
	}

	Client &client = clientIt->second;

	if (revents & (POLLERR | POLLHUP | POLLNVAL))
	{
		if (cgiFd == client.cgiStdinFd)
		{
			closeCgiFd(client.cgiStdinFd, pollManager);
			client.cgiStdinFd = -1;
			client.cgiStdinClosed = true;
			fdRemoved = 1;
		}
		else if (cgiFd == client.cgiStdoutFd)
		{
			closeCgiFd(client.cgiStdoutFd, pollManager);
			client.cgiStdoutFd = -1;
			client.cgiStdoutClosed = true;
			fdRemoved = 1;
		}
	}

	if ((revents & POLLOUT) && cgiFd == client.cgiStdinFd)
	{
		if (writeToCgi(client, pollManager) != 0)
		{
			failResponse(client, 502, "Bad Gateway", configs, pollManager);
			return (1);
		}
		else if (client.cgiStdinFd == -1)
			fdRemoved = 1;
	}

	if ((revents & POLLIN) && cgiFd == client.cgiStdoutFd)
	{
		if (readFromCgi(client, pollManager) != 0)
		{
			failResponse(client, 502, "Bad Gateway", configs, pollManager);
			return (1);
		}
		else if (client.cgiStdoutFd == -1)
			fdRemoved = 1;
	}

	if (checkFinished(client) != 0)
	{
		failResponse(client, 502, "Bad Gateway", configs, pollManager);
		return (1);
	}

	if (client.cgiStdoutClosed && client.cgiFinished)
		finishResponse(client, pollManager);

	return (fdRemoved);
}