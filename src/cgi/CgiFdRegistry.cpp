#include "CgiFdRegistry.hpp"

#include <unistd.h>

/**
 * @brief Default constructor for CgiFdRegistry.
 */
CgiFdRegistry::CgiFdRegistry() {}

/**
 * @brief Copy constructor.
 * @param other - registry to copy from
 */
CgiFdRegistry::CgiFdRegistry(const CgiFdRegistry &other)
{
	*this = other;
}

/**
 * @brief Destructor for CgiFdRegistry.
 */
CgiFdRegistry::~CgiFdRegistry() {}

/**
 * @brief Assignment operator.
 * @param other - registry to assign from
 * @return reference to this
 */
CgiFdRegistry &CgiFdRegistry::operator=(const CgiFdRegistry &other)
{
	if (this != &other)
		fdToClientFd = other.fdToClientFd;
	return (*this);
}

/**
 * @brief Check if a CGI fd is registered.
 * @param cgiFd - file descriptor to check
 * @return true if present, false otherwise
 */
bool	CgiFdRegistry::contains(int cgiFd) const
{
	return (fdToClientFd.find(cgiFd) != fdToClientFd.end());
}

/**
 * @brief Register a mapping from a CGI fd to a client fd.
 * @param cgiFd - CGI file descriptor
 * @param clientFd - client file descriptor
 */
void	CgiFdRegistry::registerFd(int cgiFd, int clientFd)
{
	fdToClientFd[cgiFd] = clientFd;
}

/**
 * @brief Unregister a CGI fd mapping.
 * @param cgiFd - CGI file descriptor to remove
 */
void	CgiFdRegistry::unregisterFd(int cgiFd)
{
	fdToClientFd.erase(cgiFd);
}

/**
 * @brief Retrieve the client fd associated with a CGI fd.
 * @param cgiFd - CGI file descriptor
 * @param clientFd - output parameter for the client fd
 * @return true if mapping exists, false otherwise
 */
bool	CgiFdRegistry::getClientFd(int cgiFd, int &clientFd) const
{
	std::map<int, int>::const_iterator it;

	it = fdToClientFd.find(cgiFd);
	if (it == fdToClientFd.end())
		return (false);
	clientFd = it->second;
	return (true);
}

/**
 * @brief Close a CGI fd, remove it from polling and unregister it.
 * @param cgiFd - file descriptor to close
 * @param pollManager - poll manager to remove the fd from
 */
void	CgiFdRegistry::closeFd(int cgiFd, PollManager &pollManager)
{
	if (cgiFd == -1)
		return;
	close(cgiFd);
	pollManager.removeFd(cgiFd);
	unregisterFd(cgiFd);
}
