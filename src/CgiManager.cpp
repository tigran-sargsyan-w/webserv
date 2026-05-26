#include "CgiManager.hpp"
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