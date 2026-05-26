#include "CgiManager.hpp"
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

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