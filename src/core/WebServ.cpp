#include "WebServ.hpp"
#include "Logger.hpp"
#include "PollEventHandler.hpp"
#include <cerrno>
#include <cstring>
#include <poll.h>
#include <vector>

/**
 * @brief Creates a WebServ instance.
 * Initializes logging for startup tracing.
 */
WebServ::WebServ()
{
	Logger::debug() << "WebServ created!\n";
}

/**
 * @brief Copies a WebServ instance.
 * @param other - source instance
 */
WebServ::WebServ(const WebServ &other)
{
	(void)other;
	Logger::debug() << "WebServ copy constructor called!\n";
}

/**
 * @brief Destroys a WebServ instance.
 */
WebServ::~WebServ()
{
	Logger::debug() << "WebServ destroyed!\n";
}

/**
 * @brief Assigns one WebServ to another.
 * @param other - source instance
 * @return reference to the current instance
 */
WebServ &WebServ::operator=(const WebServ &other)
{
	(void)other;
	Logger::debug() << "WebServ assignement operator called!\n";
	return (*this);
}

/**
 * @brief Stores server configs and initializes listeners.
 * @param servers - parsed server configurations
 * @return 0 on success, non-zero on failure
 */
int WebServ::setup(std::vector<ServerConfig> servers)
{
	Logger::debug() << "WebServ setup called!\n";
	configs = servers;
	return (listenerSocketHandler.setup(configs, pollManager));
}

/**
 * @brief Returns the smallest active poll timeout.
 * @param first - first timeout in milliseconds
 * @param second - second timeout in milliseconds
 * @return combined timeout in milliseconds
 */
static int combinePollTimeoutMs(int first, int second)
{
	if (first == 0 || second == 0)
		return (0);
	if (first < 0)
		return (second);
	if (second < 0)
		return (first);
	if (first < second)
		return (first);
	return (second);
}

/**
 * @brief Runs the main event loop.
 * Polls sockets, handles timeouts, and dispatches events.
 * @return 0 on clean exit, non-zero on failure
 */
int WebServ::run()
{
	Logger::debug() << "WebServ run called!\n";

	while (true)
	{
		std::vector<pollfd> &pollFds = pollManager.getFds();

		if (pollManager.empty())
			return (1);

		int cgiPollTimeout;
		int clientPollTimeout;
		int pollTimeout;
		int ready;

		cgiPollTimeout = cgiManager.getPollTimeoutMs(connectionManager.getClients());
		clientPollTimeout = connectionManager.getPollTimeoutMs(configs);
		pollTimeout = combinePollTimeoutMs(cgiPollTimeout, clientPollTimeout);
		ready = poll(&pollFds[0], pollFds.size(), pollTimeout);

		if (ready < 0)
		{
			if (errno == EINTR)
				continue;
			Logger::error() << "poll: " << strerror(errno) << std::endl;
			return (1);
		}
		cgiManager.checkTimeouts(connectionManager.getClients(), configs, pollManager);
		connectionManager.enforceTimeouts(configs, cgiManager, listenerSocketHandler, pollManager);
		if (ready == 0)
			continue;
		Logger::debug() << "Sockets Ready - " << ready << "\n" << std::endl;
		size_t i = 0;
		while (i < pollFds.size())
		{
			if (PollEventHandler::handle(pollFds[i], connectionManager.getClients(), configs,
										 cgiManager, listenerSocketHandler, pollManager) == PollEventHandler::ADVANCE_INDEX)
				++i;
		}
	}
}
