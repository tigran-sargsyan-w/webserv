#include "WebServ.hpp"
#include "Request.hpp"
#include "RequestHandler.hpp"
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
#include "CgiRequestHandler.hpp"
#include "ErrorResponseHandler.hpp"

#include <ctime>
#include <signal.h>
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
	ssize_t bytesRead;
	char buffer[4096];

	bytesRead = recv(client.fd, buffer, sizeof(buffer) - 1, 0);
	if (bytesRead == -1)
	{
		if (errno == EWOULDBLOCK || errno == EAGAIN)
			return (0);
		return 1;
	}
	else if (bytesRead == 0)
	{
		std::cout << "Client closed connection\n";
		client.state = CLOSING_CONNECTION;
	}
	else
	{
		buffer[bytesRead] = '\0';
		client.rawRequest.append(buffer);
		// std::cout << "Request from client:\n\n" << client.rawRequest <<
		// std::endl;
	}

	return (0);
}

static bool routeMatchesPath(const std::string &routePath, const std::string &requestPath)
{
	if (routePath == "/")
		return (true);

	if (requestPath == routePath)
		return (true);

	if (requestPath.find(routePath) != 0)
		return (false);

	if (requestPath.length() > routePath.length() &&
		requestPath[routePath.length()] == '/')
		return (true);

	return (false);
}

static const RouteConfig &findMatchingRoute(const ServerConfig &serverConfig, const std::string &requestPath)
{
	const RouteConfig *bestRoute = NULL;
	size_t bestLength = 0;

	for (std::vector<RouteConfig>::const_iterator it =
			 serverConfig.routes.begin();
		 it != serverConfig.routes.end();
		 ++it)
	{
		if (routeMatchesPath(it->path, requestPath) &&
			it->path.length() > bestLength)
		{
			bestRoute = &(*it);
			bestLength = it->path.length();
		}
	}

	if (bestRoute != NULL)
		return (*bestRoute);

	return (serverConfig.routes.front());
}

static std::string getPathWithoutQuery(const std::string &path)
{
	size_t questionMark = path.find('?');

	if (questionMark == std::string::npos)
		return (path);
	return (path.substr(0, questionMark));
}

int WebServ::SendToClient(Client &client)
{
	ssize_t bytesSent;
	size_t remaining;
	const char *data;

	if (!client.responseReady)
	{
		std::string cleanPath = getPathWithoutQuery(client.request.getPath());

		const RouteConfig &route = findMatchingRoute(configs[client.serverIndex], cleanPath);

		std::cout << "Matched route: " << route.path << std::endl;

		if (CgiRequestHandler::isCgiRequest(client.request, route))
		{
			if (startCgiForClient(client, route) != 0)
				return (1);
			return (0);
		}

		Response response = RequestHandler::handleRequest(
			client.request, route, configs[client.serverIndex], client.getRemoteAddr());

		client.responseBuffer = response.toString();
		client.bytesSent = 0;
		client.responseReady = true;

		std::cout << "Response to client:\n\n"
				  << client.responseBuffer << std::endl;
	}
	if (client.bytesSent >= client.responseBuffer.size())
	{
		client.state = CLOSING_CONNECTION;
		return (0);
	}

	remaining = client.responseBuffer.size() - client.bytesSent;
	data = client.responseBuffer.c_str() + client.bytesSent;
	bytesSent = send(client.fd, data, remaining, 0);

	if (bytesSent == -1)
	{
		if (errno == EWOULDBLOCK || errno == EAGAIN)
			return (0);
		if (errno == EINTR)
			return (0);
		std::cout << "send: " << strerror(errno) << std::endl;
		return (1);
	}

	if (bytesSent == 0)
		return (0);

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
		std::cerr << "Error binding socket\n" << std::strerror(errno) << "\n";
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

		struct pollfd tmpPollfd;
		tmpPollfd.fd = listeningSocket;
		tmpPollfd.events = POLLIN;
		tmpPollfd.revents = 0;
		this->pollFds.push_back(tmpPollfd);

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
	struct pollfd tmpPollfd;
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

	tmpPollfd.fd = clientSocket;
	tmpPollfd.events = POLLIN;
	tmpPollfd.revents = 0;
	this->pollFds.push_back(tmpPollfd);

	clients.insert(std::make_pair(clientSocket, Client(clientSocket)));
	clientIt = clients.find(clientSocket);
	if (clientIt != clients.end())
	{
		clientIt->second.setRemoteAddr(ipToString(clientAddress.sin_addr.s_addr));
		clientIt->second.serverIndex = listenerFdToIndex[listeningSocket];
	}
	return (clientSocket);
}

void WebServ::registerPollFd(int fd, short events)
{
	struct pollfd tmpPollfd;

	tmpPollfd.fd = fd;
	tmpPollfd.events = events;
	tmpPollfd.revents = 0;
	pollFds.push_back(tmpPollfd);
}

void WebServ::setPollEvents(int fd, short events)
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

bool WebServ::isCgiFd(int fd) const
{
	return (cgiFdToClientFd.find(fd) != cgiFdToClientFd.end());
}

void WebServ::removePollfd(int fd)
{
	for (size_t i = 0; i < this->pollFds.size(); i++)
	{
		if (this->pollFds[i].fd == fd)
		{
			this->pollFds.erase(this->pollFds.begin() + i);
			return;
		}
	}
}

bool WebServ::isListeningFd(int fd)
{
	return (this->listenerFdToIndex.find(fd) != this->listenerFdToIndex.end());
}

void WebServ::closeAndRemoveFd(int fd)
{
	close(fd);
	removePollfd(fd);
	clients.erase(fd);
	listenerFdToIndex.erase(fd);
}

int WebServ::startCgiForClient(Client &client, const RouteConfig &route)
{
	CgiContext context;
	CgiProcess process;
	const ServerConfig &server = configs[client.serverIndex];

	context = CgiRequestHandler::buildContext(client.request, route, server, client.getRemoteAddr());

	if (context.executable.empty() || context.scriptPath.empty())
	{
		Response error = ErrorResponseHandler::build(403, "Forbidden", server);

		client.responseBuffer = error.toString();
		client.bytesSent = 0;
		client.responseReady = true;
		client.state = WRITING;
		setPollEvents(client.fd, POLLOUT);
		return (0);
	}

	if (CgiHandler::startCgi(context, process) != 0)
	{
		Response error = ErrorResponseHandler::build(502, "Bad Gateway", server);

		client.responseBuffer = error.toString();
		client.bytesSent = 0;
		client.responseReady = true;
		client.state = WRITING;
		setPollEvents(client.fd, POLLOUT);
		return (1);
	}

	if (setNonBlocking(process.stdinFd) || setNonBlocking(process.stdoutFd))
	{
		close(process.stdinFd);
		close(process.stdoutFd);
		kill(process.pid, SIGKILL);
		waitpid(process.pid, NULL, WNOHANG);

		Response error = ErrorResponseHandler::build(500, "Internal Server Error", server);

		client.responseBuffer = error.toString();
		client.bytesSent = 0;
		client.responseReady = true;
		client.state = WRITING;
		setPollEvents(client.fd, POLLOUT);
		return (1);
	}

	client.cgiPid = process.pid;
	client.cgiStdinFd = process.stdinFd;
	client.cgiStdoutFd = process.stdoutFd;
	client.cgiInputBuffer = context.requestBody;
	client.cgiInputSent = 0;
	client.cgiOutputBuffer.clear();
	client.cgiStdinClosed = false;
	client.cgiStdoutClosed = false;
	client.cgiFinished = false;
	client.cgiStartTime = std::time(NULL);

	cgiFdToClientFd[client.cgiStdoutFd] = client.fd;
	registerPollFd(client.cgiStdoutFd, POLLIN);

	if (client.cgiInputBuffer.empty())
	{
		close(client.cgiStdinFd);
		client.cgiStdinFd = -1;
		client.cgiStdinClosed = true;
		client.state = CGI_READING;
	}
	else
	{
		cgiFdToClientFd[client.cgiStdinFd] = client.fd;
		registerPollFd(client.cgiStdinFd, POLLOUT);
		client.state = CGI_WRITING;
	}

	setPollEvents(client.fd, 0);
	return (0);
}

int WebServ::writeToCgi(Client &client)
{
	size_t remaining;
	ssize_t bytesWritten;

	if (client.cgiStdinFd == -1 || client.cgiStdinClosed)
		return (0);

	remaining = client.cgiInputBuffer.size() - client.cgiInputSent;
	if (remaining == 0)
	{
		closeCgiFd(client.cgiStdinFd);
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
		closeCgiFd(client.cgiStdinFd);
		client.cgiStdinFd = -1;
		client.cgiStdinClosed = true;
	}

	return (0);
}

int WebServ::readFromCgi(Client &client)
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
		closeCgiFd(client.cgiStdoutFd);
		client.cgiStdoutFd = -1;
		client.cgiStdoutClosed = true;
		return (0);
	}

	client.cgiOutputBuffer.append(buffer, bytesRead);
	return (0);
}

void WebServ::closeCgiFd(int fd)
{
	if (fd == -1)
		return;

	close(fd);
	removePollfd(fd);
	cgiFdToClientFd.erase(fd);
}

int WebServ::checkCgiFinished(Client &client)
{
	int status;
	pid_t result;

	if (client.cgiPid <= 0)
		return (0);

	result = waitpid(client.cgiPid, &status, WNOHANG);
	if (result == 0)
		return (0);

	if (result == client.cgiPid)
	{
		client.cgiPid = -1;
		client.cgiFinished = true;

		if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
			return (1);
		if (WIFSIGNALED(status))
			return (1);

		return (0);
	}

	return (1);
}

void WebServ::finishCgiResponse(Client &client)
{
	Response response;

	response = CgiRequestHandler::buildResponse(client.cgiOutputBuffer);

	client.responseBuffer = response.toString();
	client.bytesSent = 0;
	client.responseReady = true;
	client.state = WRITING;

	setPollEvents(client.fd, POLLOUT);
}

void WebServ::cleanupCgi(Client &client)
{
	if (client.cgiStdinFd != -1)
	{
		closeCgiFd(client.cgiStdinFd);
		client.cgiStdinFd = -1;
	}

	if (client.cgiStdoutFd != -1)
	{
		closeCgiFd(client.cgiStdoutFd);
		client.cgiStdoutFd = -1;
	}

	if (client.cgiPid > 0)
	{
		kill(client.cgiPid, SIGKILL);
		waitpid(client.cgiPid, NULL, WNOHANG);
		client.cgiPid = -1;
	}

	client.cgiStdinClosed = true;
	client.cgiStdoutClosed = true;
}

int WebServ::handleCgiEvent(int cgiFd, short revents)
{
	std::map<int, int>::iterator mapIt;
	std::map<int, Client>::iterator clientIt;

	mapIt = cgiFdToClientFd.find(cgiFd);
	if (mapIt == cgiFdToClientFd.end())
		return (1);

	clientIt = clients.find(mapIt->second);
	if (clientIt == clients.end())
	{
		close(cgiFd);
		removePollfd(cgiFd);
		cgiFdToClientFd.erase(cgiFd);
		return (1);
	}

	Client &client = clientIt->second;

	if (revents & (POLLERR | POLLHUP | POLLNVAL))
	{
		if (cgiFd == client.cgiStdinFd)
		{
			closeCgiFd(client.cgiStdinFd);
			client.cgiStdinFd = -1;
			client.cgiStdinClosed = true;
		}
		else if (cgiFd == client.cgiStdoutFd)
		{
			closeCgiFd(client.cgiStdoutFd);
			client.cgiStdoutFd = -1;
			client.cgiStdoutClosed = true;
		}
	}

	if ((revents & POLLOUT) && cgiFd == client.cgiStdinFd)
	{
		if (writeToCgi(client) != 0)
			cleanupCgi(client);
	}

	if ((revents & POLLIN) && cgiFd == client.cgiStdoutFd)
	{
		if (readFromCgi(client) != 0)
			cleanupCgi(client);
	}

	checkCgiFinished(client);

	if (client.cgiStdoutClosed && client.cgiFinished)
		finishCgiResponse(client);

	return (0);
}

int WebServ::run()
{
	std::cout << "WebServ run called!\n";

	while (true)
	{
		int ready = poll(&this->pollFds[0], this->pollFds.size(), -1);
		if (ready < 0)
		{
			if (errno == EINTR)
				continue;
			std::cerr << "poll: " << strerror(errno) << std::endl;
			return (1);
		}
		std::cout << "Sockets Ready - " << ready << "\n" << std::endl;

		// PollFds loop

		size_t i = 0;
		while (i < pollFds.size())
		{
			int curFD = pollFds[i].fd;

			if (isCgiFd(curFD))
			{
				handleCgiEvent(curFD, pollFds[i].revents);
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
				if (this->pollFds[i].revents & POLLIN)
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

					inspector.inspectRequest(curClient.getRawRequest());
					if (inspector.status == COMPLETED)
					{
						parser.parse(curClient.getRawRequest(), curClient.request);
					}
					else if (inspector.status == NEED_MORE_DATA)
					{
						++i;
						continue;
					}
					else
					{
						// TODO: RequestHandler for errors and close connection
						closeAndRemoveFd(curFD);
						continue;
					}

					// TODO: if request is valid set as POLLOUT
					this->pollFds[i].events = POLLOUT;
				}
				if (this->pollFds[i].revents & POLLOUT)
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
