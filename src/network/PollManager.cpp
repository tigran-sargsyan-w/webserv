#include "PollManager.hpp"
#include <cstddef>

/**
 * @brief Create an empty poll manager.
 */
PollManager::PollManager() {}

/**
 * @brief Copy poll manager state.
 * @param other - source manager
 */
PollManager::PollManager(const PollManager &other)
{
	*this = other;
}

/**
 * @brief Destroy the poll manager.
 */
PollManager::~PollManager() {}

/**
 * @brief Copy poll manager state.
 * @param other - source manager
 * @return reference to this manager
 */
PollManager &PollManager::operator=(const PollManager &other)
{
	if (this != &other)
		pollFds = other.pollFds;
	return (*this);
}

/**
 * @brief Check whether no descriptors are registered.
 * @return true if empty
 */
bool PollManager::empty() const
{
	return (pollFds.empty());
}

/**
 * @brief Add a descriptor to the poll set.
 * @param fd - descriptor to add
 * @param events - monitored events
 */
void PollManager::addFd(int fd, short events)
{
	struct pollfd newPollFd;

	newPollFd.fd = fd;
	newPollFd.events = events;
	newPollFd.revents = 0;
	pollFds.push_back(newPollFd);
}

/**
 * @brief Remove a descriptor from the poll set.
 * @param fd - descriptor to remove
 */
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

/**
 * @brief Update monitored events for a descriptor.
 * @param fd - descriptor to update
 * @param events - new monitored events
 */
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

/**
 * @brief Get the number of registered descriptors.
 * @return poll set size
 */
size_t PollManager::size() const
{
	return (pollFds.size());
}

/**
 * @brief Get the mutable poll descriptor list.
 * @return registered poll fds
 */
std::vector<pollfd> &PollManager::getFds()
{
	return (pollFds);
}

/**
 * @brief Get the immutable poll descriptor list.
 * @return registered poll fds
 */
const std::vector<pollfd> &PollManager::getFds() const
{
	return (pollFds);
}