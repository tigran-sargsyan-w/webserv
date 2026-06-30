#include "CgiFdRegistry.hpp"

#include <unistd.h>

CgiFdRegistry::CgiFdRegistry() {}

CgiFdRegistry::CgiFdRegistry(const CgiFdRegistry &other)
{
	*this = other;
}

CgiFdRegistry::~CgiFdRegistry() {}

CgiFdRegistry &CgiFdRegistry::operator=(const CgiFdRegistry &other)
{
	if (this != &other)
		fdToClientFd = other.fdToClientFd;
	return (*this);
}

bool	CgiFdRegistry::contains(int cgiFd) const
{
	return (fdToClientFd.find(cgiFd) != fdToClientFd.end());
}

void	CgiFdRegistry::registerFd(int cgiFd, int clientFd)
{
	fdToClientFd[cgiFd] = clientFd;
}

void	CgiFdRegistry::unregisterFd(int cgiFd)
{
	fdToClientFd.erase(cgiFd);
}

bool	CgiFdRegistry::getClientFd(int cgiFd, int &clientFd) const
{
	std::map<int, int>::const_iterator it;

	it = fdToClientFd.find(cgiFd);
	if (it == fdToClientFd.end())
		return (false);
	clientFd = it->second;
	return (true);
}

void	CgiFdRegistry::closeFd(int cgiFd, PollManager &pollManager)
{
	if (cgiFd == -1)
		return;
	close(cgiFd);
	pollManager.removeFd(cgiFd);
	unregisterFd(cgiFd);
}
