#include "ListenerSocketHandler.hpp"
#include "Logger.hpp"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sstream>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>

/**
 * @brief Create an empty listener socket handler.
 */
ListenerSocketHandler::ListenerSocketHandler()
{
}

/**
 * @brief Copy listener socket state from another handler.
 * @param other - source handler
 */
ListenerSocketHandler::ListenerSocketHandler(const ListenerSocketHandler &other)
{
	*this = other;
}

/**
 * @brief Destroy the listener socket handler.
 */
ListenerSocketHandler::~ListenerSocketHandler()
{
}

/**
 * @brief Copy listener socket state.
 * @param other - source handler
 * @return reference to this handler
 */
ListenerSocketHandler &ListenerSocketHandler::operator=(
	const ListenerSocketHandler &other)
{
	if (this != &other)
		listenerFdToIndex = other.listenerFdToIndex;
	return (*this);
}

/**
 * @brief Set a socket to non-blocking mode.
 * @param fd - socket descriptor
 * @return 0 on success, 1 on failure
 */
int ListenerSocketHandler::setNonBlocking(int fd) const
{
	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
	{
		Logger::error() << "fcntl: " << strerror(errno) << "\n";
		return (1);
	}
	return (0);
}

/**
 * @brief Create a listening socket with reuse enabled.
 * @return listening socket descriptor, or -1 on failure
 */
int ListenerSocketHandler::initListeningSocket(void) const
{
	int listeningSocket;
	int opt;

	listeningSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (listeningSocket == -1)
	{
		Logger::error() << "socket() failed: " << std::strerror(errno) << "\n";
		return (-1);
	}
	Logger::debug() << "Server socked created, FD = " << listeningSocket << "\n";
	opt = 1;
	if (setsockopt(listeningSocket, SOL_SOCKET, SO_REUSEADDR,
			&opt, sizeof(opt)) == -1)
	{
		Logger::error() << "setsockopt() failed: " << std::strerror(errno) << "\n";
		close(listeningSocket);
		return (-1);
	}
	return (listeningSocket);
}

/**
 * @brief Bind a listening socket to a configured address.
 * @param listeningSocket - socket descriptor
 * @param config - server configuration
 * @return 0 on success, 1 on failure
 */
int ListenerSocketHandler::bindSockAddress(int listeningSocket,
	const ServerConfig &config) const
{
	struct addrinfo	hints;
	struct addrinfo	*res;
	std::stringstream	ss;
	std::string		portStr;
	const char		*hostCstr;
	int				ret;

	res = NULL;
	std::memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	ss << config.listen.port;
	portStr = ss.str();
	hostCstr = NULL;
	if (!config.listen.host.empty())
		hostCstr = config.listen.host.c_str();
	ret = getaddrinfo(hostCstr, portStr.c_str(), &hints, &res);
	if (ret)
	{
		Logger::error() << "getaddrinfo: " << gai_strerror(ret) << "\n";
		close(listeningSocket);
		return (1);
	}
	if (bind(listeningSocket, res->ai_addr, res->ai_addrlen) == -1)
	{
		Logger::error() << "Error binding socket\n" << std::strerror(errno) << "\n";
		freeaddrinfo(res);
		close(listeningSocket);
		return (1);
	}
	freeaddrinfo(res);
	return (0);
}

/**
 * @brief Set up one server listener socket.
 * @param config - server configuration
 * @param configIndex - index of the configuration
 * @param pollManager - poll manager to register the socket
 * @return 0 on success, 1 on failure
 */
int ListenerSocketHandler::setupSingleListener(const ServerConfig &config,
	size_t configIndex, PollManager &pollManager)
{
	int listeningSocket;

	listeningSocket = initListeningSocket();
	if (listeningSocket == -1)
		return (1);
	if (bindSockAddress(listeningSocket, config))
		return (1);
	if (listen(listeningSocket, 10) == -1)
	{
		Logger::error() << "Error on socket " << configIndex << " listening\n";
		close(listeningSocket);
		return (1);
	}
	if (setNonBlocking(listeningSocket))
	{
		Logger::error() << "Error setting socket " << configIndex
			<< " as Non blocking\n";
		close(listeningSocket);
		return (1);
	}
	listenerFdToIndex[listeningSocket] = configIndex;
	pollManager.addFd(listeningSocket, POLLIN);
	Logger::info() << "Listening on " << config.listen.host << ":"
		<< config.listen.port << "\n";
	return (0);
}

/**
 * @brief Set up listeners for all configured servers.
 * @param configs - server configuration list
 * @param pollManager - poll manager to register sockets
 * @return 0 on success, 1 if every listener failed
 */
int ListenerSocketHandler::setup(const std::vector<ServerConfig> &configs,
	PollManager &pollManager)
{
	size_t	i;
	bool	stop;

	stop = true;
	i = 0;
	while (i < configs.size())
	{
		if (setupSingleListener(configs[i], i, pollManager))
			Logger::error() << "Server Block " << i << " setup failed!\n";
		else
			stop = false;
		++i;
	}
	if (stop)
		return (1);
	return (0);
}

/**
 * @brief Convert an IPv4 address to dotted decimal notation.
 * @param address - network-order address
 * @return printable IP string
 */
std::string ListenerSocketHandler::ipToString(unsigned int address) const
{
	std::ostringstream	oss;
	unsigned int		hostAddress;

	hostAddress = ntohl(address);
	oss << ((hostAddress >> 24) & 255) << "." << ((hostAddress >> 16) & 255)
		<< "." << ((hostAddress >> 8) & 255) << "." << (hostAddress & 255);
	return (oss.str());
}

/**
 * @brief Accept a new client connection.
 * @param listeningSocket - listener descriptor
 * @param clients - tracked clients map
 * @param pollManager - poll manager to register the client socket
 * @return accepted client descriptor, or -1 on failure
 */
int ListenerSocketHandler::acceptConnection(int listeningSocket,
	std::map<int, Client> &clients, PollManager &pollManager)
{
	sockaddr_in					clientAddress;
	socklen_t					clientAddressLength;
	int							clientSocket;
	std::map<int, Client>::iterator	clientIt;

	clientAddressLength = sizeof(clientAddress);
	clientSocket = accept(listeningSocket, (sockaddr *)&clientAddress,
		&clientAddressLength);
	if (clientSocket == -1)
	{
		if (errno != EAGAIN && errno != EWOULDBLOCK)
			Logger::error() << "accept: " << strerror(errno) << std::endl;
		return (-1);
	}
	if (setNonBlocking(clientSocket))
	{
		Logger::error() << "fcntl: " << strerror(errno) << std::endl;
		close(clientSocket);
		return (-1);
	}
	pollManager.addFd(clientSocket, POLLIN);
	clients.insert(std::make_pair(clientSocket, Client(clientSocket)));
	clientIt = clients.find(clientSocket);
	if (clientIt != clients.end())
	{
		clientIt->second.setRemoteAddr(ipToString(clientAddress.sin_addr.s_addr));
		clientIt->second.serverIndex = listenerFdToIndex[listeningSocket];
	}
	return (clientSocket);
}

/**
 * @brief Check whether a descriptor belongs to a listener socket.
 * @param fd - file descriptor to test
 * @return true if the descriptor is a listener
 */
bool ListenerSocketHandler::isListeningFd(int fd) const
{
	return (listenerFdToIndex.find(fd) != listenerFdToIndex.end());
}

/**
 * @brief Remove a listener socket from the registry.
 * @param fd - listener descriptor
 */
void ListenerSocketHandler::removeFd(int fd)
{
	listenerFdToIndex.erase(fd);
}
