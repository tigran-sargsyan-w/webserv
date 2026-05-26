#include "CgiManager.hpp"

CgiManager::CgiManager()
{
}

CgiManager::CgiManager(const CgiManager &other)
{
	*this = other;
}

CgiManager::~CgiManager()
{
}

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