#include "WebServ.hpp"
#include "Request.hpp"
#include "RequestInspector.hpp"
#include "RequestParser.hpp"
#include "Response.hpp"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netdb.h>
#include <poll.h>
#include <sstream>
#include <sys/poll.h>
#include <sys/socket.h>
#include <utility>

#include "CgiHandler.hpp"
#include "ClientResponseApplier.hpp"
#include "RequestDispatcher.hpp"
#include "ErrorResponseHandler.hpp"

#include <ctime>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>

WebServ::WebServ()
{
	std::cout << "WebServ created!\n";
}

WebServ::WebServ(const WebServ &other)
{
	(void)other;
	std::cout << "WebServ copy constructor called!\n";
}

WebServ::~WebServ()
{
	std::cout << "WebServ destroyed!\n";
}

WebServ &WebServ::operator=(const WebServ &other)
{
	(void)other;
	std::cout << "WebServ assignement operator called!\n";
	return (*this);
}

int WebServ::setNonBlocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
	{
		std::cerr << "fcntl: " << strerror(errno) << "\n";
		return (1);
	}
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
	{
		std::cerr << "fcntl: " << strerror(errno) << "\n";
		return (1);
	}
	return (0);
}

int WebServ::readFromClient(Client &client)
{
	ssize_t	bytesRead;
	char	buffer[4096];

	bytesRead = recv(client.fd, buffer, sizeof(buffer), 0);
	if (bytesRead < 0)
	{
		std::cerr << "Failed to read from client fd " << client.fd << std::endl;
		client.state = CLOSING_CONNECTION;
		return (1);
	}
	if (bytesRead == 0)
	{
		std::cout << "Client closed connection" << std::endl;
		client.state = CLOSING_CONNECTION;
		return (0);
	}
	client.rawRequest.append(buffer, static_cast<size_t>(bytesRead));
	return (0);
}

int WebServ::discardRequestBody(Client &client)
{
	ssize_t bytesRead;
	char buffer[4096];
	size_t readSize;

	if (client.bodyBytesToDiscard == 0)
	{
		client.state = WRITING;
		return (0);
	}
	readSize = sizeof(buffer);
	if (client.bodyBytesToDiscard < readSize)
		readSize = client.bodyBytesToDiscard;
	bytesRead = recv(client.fd, buffer, readSize, 0);
	if (bytesRead < 0)
	{
		std::cerr << "Failed to discard request body from client fd "
				  << client.fd << std::endl;
		client.state = CLOSING_CONNECTION;
		return (1);
	}
	if (bytesRead == 0)
	{
		client.bodyBytesToDiscard = 0;
		client.state = WRITING;
		return (0);
	}
	if (static_cast<size_t>(bytesRead) >= client.bodyBytesToDiscard)
	{
		client.bodyBytesToDiscard = 0;
		client.state = WRITING;
	}
	else
		client.bodyBytesToDiscard -= static_cast<size_t>(bytesRead);
	return (0);
}

int WebServ::SendToClient(Client &client)
{
	RequestDispatcher::Result dispatchResult;
	ssize_t bytesSent;
	size_t remaining;
	const char *data;

	if (!client.responseReady)
	{
		dispatchResult = RequestDispatcher::dispatch(client, configs[client.serverIndex],
											 cgiManager, pollManager);

		if (dispatchResult == RequestDispatcher::DISPATCH_FAILED)
			return (1);
		if (dispatchResult == RequestDispatcher::ASYNC_STARTED)
			return (0);
	}

	if (client.bytesSent >= client.responseBuffer.size())
	{
		client.state = CLOSING_CONNECTION;
		return (0);
	}

	remaining = client.responseBuffer.size() - client.bytesSent;
	data = client.responseBuffer.c_str() + client.bytesSent;
	bytesSent = send(client.fd, data, remaining, 0);

	if (bytesSent <= 0)
	{
		std::cerr << "Failed to send response to client fd "
				  << client.fd << std::endl;
		client.state = CLOSING_CONNECTION;
		return (1);
	}

	client.bytesSent += static_cast<size_t>(bytesSent);

	if (client.bytesSent >= client.responseBuffer.size())
		client.state = CLOSING_CONNECTION;

	return (0);
}

int WebServ::initListeningSocket()
{
	int tmpSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (tmpSocket == -1)
	{
		std::cerr << "socket() failed: " << std::strerror(errno) << "\n";
		return (-1);
	}

	std::cout << "Server socked created, FD = " << tmpSocket << "\n";

	int opt = 1;
	if (setsockopt(
			tmpSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) ==
		-1)
	{
		std::cerr << "setsockopt() failed: " << std::strerror(errno) << "\n";
		close(tmpSocket);
		return (-1);
	}
	return (tmpSocket);
}

int WebServ::bindSockAddress(int listeningSocket, size_t configIndex)
{

	struct addrinfo hints;
	struct addrinfo *res = NULL;

	std::memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;

	std::stringstream ss;
	ss << configs[configIndex].listen.port;
	std::string port_str = ss.str();

	const char *host_cstr = NULL;
	if (!configs[configIndex].listen.host.empty())
	{
		host_cstr = configs[configIndex].listen.host.c_str();
	}

	int ret = getaddrinfo(host_cstr, port_str.c_str(), &hints, &res);
	if (ret)
	{
		std::cerr << "getaddrinfo: " << gai_strerror(ret) << "\n";
		return (1);
	}

	if (bind(listeningSocket, res->ai_addr, res->ai_addrlen) == -1)
	{
		std::cerr << "Error binding socket\n"
				  << std::strerror(errno) << "\n";
		freeaddrinfo(res);
		close(listeningSocket);
		return (1);
	}
	freeaddrinfo(res);
	return (0);
}

int WebServ::setup(std::vector<ServerConfig> servers)
{
	// TODO: add created ListenerSockets to pollfds and to map
	std::cout << "WebServ setup called!\n";

	this->configs = servers; // Refactor instead of copying?
	bool stop = true;
	for (size_t i = 0; i < servers.size(); ++i)
	{
		ServerConfig &tmpConfig = servers[i];

		// 1. Create socket
		int listeningSocket = initListeningSocket();
		if (listeningSocket == -1)
		{
			std::cerr << "Server Block " << i << " setup failed!\n";
			continue;
		}

		// 2. Setup address for socket
		if (bindSockAddress(listeningSocket, i))
		{
			std::cerr << "Server Block " << i << " setup failed!\n";
			continue;
		}

		// 3. Socket listening
		if (listen(listeningSocket, 10) == -1)
		{
			std::cerr << "Error on socket " << i << " listening\n";
			close(listeningSocket);
			continue;
		}
		if (setNonBlocking(listeningSocket))
		{
			std::cerr << "Error setting socket " << i << " as Non blocking\n";
			close(listeningSocket);
			continue;
		}

		listenerFdToIndex[listeningSocket] = i;

		pollManager.addFd(listeningSocket, POLLIN);

		std::cout << "Listening on " << tmpConfig.listen.host << ":"
				  << tmpConfig.listen.port << "\n";
		stop = false;
	}
	if (stop)
		return (1);
	return (0);
}

static std::string ipToString(uint32_t address)
{
	std::ostringstream oss;
	uint32_t hostAddress;

	hostAddress = ntohl(address);
	oss << ((hostAddress >> 24) & 255) << "." << ((hostAddress >> 16) & 255)
		<< "." << ((hostAddress >> 8) & 255) << "." << (hostAddress & 255);
	return (oss.str());
}

int WebServ::acceptConnection(int listeningSocket)
{
	sockaddr_in clientAddress;
	socklen_t clientAddressLength;
	int clientSocket;
	std::map<int, Client>::iterator clientIt;

	clientAddressLength = sizeof(clientAddress);
	clientSocket = accept(listeningSocket, (sockaddr *)&clientAddress, &clientAddressLength);
	if (clientSocket == -1)
	{
		if (errno != EAGAIN && errno != EWOULDBLOCK)
			std::cerr << "accept: " << strerror(errno) << std::endl;
		return (-1);
	}

	if (setNonBlocking(clientSocket))
	{
		std::cerr << "fcntl: " << strerror(errno) << std::endl;
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

bool WebServ::isListeningFd(int fd)
{
	return (this->listenerFdToIndex.find(fd) != this->listenerFdToIndex.end());
}

static bool hasActiveCgi(const Client &client)
{
	return (client.cgi.isActive());
}

void WebServ::closeAndRemoveFd(int fd)
{
	std::map<int, Client>::iterator clientIt;

	clientIt = clients.find(fd);
	if (clientIt != clients.end())
	{
		if (hasActiveCgi(clientIt->second))
		{
			std::cout << "Cleaning CGI for disconnected client fd " << fd << std::endl;
			cgiManager.cleanup(clientIt->second, pollManager);
		}
		clients.erase(clientIt);
	}

	close(fd);
	pollManager.removeFd(fd);
	listenerFdToIndex.erase(fd);
}

static std::string getInspectorErrorMessage(InspectRequestStatus status)
{
	if (status == BAD_REQUEST)
		return ("Bad Request");
	if (status == REQUEST_TOO_LARGE)
		return ("Payload Too Large");
	if (status == URI_TOO_LONG)
		return ("URI Too Long");
	if (status == HEADER_TOO_LARGE)
		return ("Request Header Fields Too Large");
	if (status == NOT_IMPLEMENTED)
		return ("Not Implemented");
	return ("Bad Request");
}

static void prepareInspectorErrorResponse(Client &client, const ServerConfig &server, InspectRequestStatus status)
{
	Response response;
	int statusCode;

	statusCode = static_cast<int>(status);
	if (statusCode < 400 || statusCode > 599)
		statusCode = 400;

	response = ErrorResponseHandler::build(statusCode, getInspectorErrorMessage(status), server);
	ClientResponseApplier::apply(client, response);
}

static void prepareBodyDiscard(Client &client, const RequestInspection &inspection)
{
	size_t currentBodySize;

	client.bodyBytesToDiscard = 0;
	if (!inspection.hasContentLength)
		return;
	currentBodySize = 0;
	if (client.rawRequest.length() > inspection.bodyStart)
		currentBodySize = client.rawRequest.length() - inspection.bodyStart;
	if (currentBodySize < inspection.contentLength)
	{
		client.bodyBytesToDiscard = inspection.contentLength - currentBodySize;
		client.state = DISCARDING_BODY;
	}
}

int WebServ::run()
{
	std::cout << "WebServ run called!\n";

	while (true)
	{
		std::vector<pollfd> &pollFds = pollManager.getFds();

		if (pollManager.empty())
			return (1);

		int ready = poll(&pollFds[0], pollFds.size(), cgiManager.getPollTimeoutMs(clients));

		if (ready < 0)
		{
			if (errno == EINTR)
				continue;
			std::cerr << "poll: " << strerror(errno) << std::endl;
			return (1);
		}
		// Check CGI timeouts on each loop iteration
		cgiManager.checkTimeouts(clients, configs, pollManager);

		// No events, continue polling
		if (ready == 0)
			continue;

		std::cout << "Sockets Ready - " << ready << "\n"
				  << std::endl;

		// PollFds loop

		size_t i = 0;
		while (i < pollFds.size())
		{
			int curFD = pollFds[i].fd;

			if (cgiManager.isCgiFd(curFD))
			{
				if (cgiManager.handleEvent(curFD, pollFds[i].revents, clients, configs, pollManager) == 0)
					++i;
				continue;
			}

			if (pollFds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
			{
				closeAndRemoveFd(curFD);
				continue;
			}

			// Accept connections

			if (isListeningFd(curFD))
			{
				if (pollFds[i].revents & POLLIN)
					acceptConnection(curFD);

				++i;
				continue;
			}
			else
			{
				std::map<int, Client>::iterator clientIt = clients.find(curFD);
				if (clientIt == clients.end())
				{
					++i;
					continue;
				}

				Client &curClient = clientIt->second;
				if (curClient.state == DISCARDING_BODY)
				{
					if (pollFds[i].revents & POLLIN)
					{
						discardRequestBody(curClient);
						if (curClient.state == CLOSING_CONNECTION)
						{
							closeAndRemoveFd(curFD);
							continue;
						}
						if (curClient.state == WRITING)
							pollManager.setEvents(curFD, POLLOUT);
					}
					++i;
					continue;
				}
				if (curClient.state == CGI_WRITING || curClient.state == CGI_READING)
				{
					if (pollFds[i].revents & POLLIN)
					{
						readFromClient(curClient);
						if (curClient.state == CLOSING_CONNECTION)
						{
							closeAndRemoveFd(curFD);
							continue;
						}
						curClient.rawRequest.clear();
					}
					++i;
					continue;
				}
				if (pollFds[i].revents & POLLIN)
				{
					curClient.state = READING;
					readFromClient(curClient);
					if (curClient.state == CLOSING_CONNECTION)
					{
						closeAndRemoveFd(curFD);
						continue;
					}

					RequestParser parser;
					RequestInspector inspector;
					RequestInspection inspection;

					inspection = inspector.inspectRequest(curClient.getRawRequest(),
						configs[curClient.serverIndex].clientMaxBodySize);
					if (inspection.status == COMPLETED)
					{
						parser.parse(curClient.getRawRequest(), curClient.request, inspection);
					}
					else if (inspection.status == NEED_MORE_DATA)
					{
						++i;
						continue;
					}
					else
					{
						prepareInspectorErrorResponse(curClient, configs[curClient.serverIndex],
							inspection.status);
						if (inspection.status == REQUEST_TOO_LARGE)
							prepareBodyDiscard(curClient, inspection);
						if (curClient.state == DISCARDING_BODY)
							pollManager.setEvents(curFD, POLLIN);
						else
							pollManager.setEvents(curFD, POLLOUT);
						++i;
						continue;
					}

					// TODO: if request is valid set as POLLOUT
					pollManager.setEvents(curFD, POLLOUT);
				}
				if (pollFds[i].revents & POLLOUT)
				{
					curClient.state = WRITING;
					SendToClient(curClient);
					if (curClient.state == CLOSING_CONNECTION)
					{
						closeAndRemoveFd(curFD);
						continue;
					}
				}
			}
			++i;
		}
	}
}
