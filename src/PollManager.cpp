#include "PollManager.hpp"
#include <cstddef>

PollManager::PollManager() {}

PollManager::PollManager(const PollManager &other)
{
	*this = other;
}

PollManager::~PollManager() {}

PollManager &PollManager::operator=(const PollManager &other)
{
	if (this != &other)
		pollFds = other.pollFds;
	return (*this);
}

void PollManager::addFd(int fd, short events)
{
	struct pollfd newPollFd;

	newPollFd.fd = fd;
	newPollFd.events = events;
	newPollFd.revents = 0;
	pollFds.push_back(newPollFd);
}

void PollManager::removeFd(int fd)
{
	for (size_t i = 0; i < pollFds.size(); ++i)
	{
		if (pollFds[i].fd == fd)
		{
			pollFds.erase(pollFds.begin() + i);
			return;
		}
	}
}

void PollManager::setEvents(int fd, short events)
{
	for (size_t i = 0; i < pollFds.size(); ++i)
	{
		if (pollFds[i].fd == fd)
		{
			pollFds[i].events = events;
			pollFds[i].revents = 0;
			return;
		}
	}
}

std::vector<pollfd> &PollManager::getFds()
{
	return (pollFds);
}

const std::vector<pollfd> &PollManager::getFds() const
{
	return (pollFds);
}