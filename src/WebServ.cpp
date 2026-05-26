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
#include <sys/stat.h>

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

		Response response = RequestHandler::handleRequest(client.request, route, configs[client.serverIndex]);

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
	return (client.cgiPid > 0 || client.cgiStdinFd != -1 || client.cgiStdoutFd != -1);
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

static std::string getCgiValidationMessage(int statusCode)
{
	if (statusCode == 403)
		return ("Forbidden");
	if (statusCode == 404)
		return ("Not Found");
	if (statusCode == 502)
		return ("Bad Gateway");
	return ("Internal Server Error");
}

static void prepareCgiErrorResponse(Client &client, const ServerConfig &server, int statusCode)
{
	Response error;

	error = ErrorResponseHandler::build(statusCode, getCgiValidationMessage(statusCode), server);
	client.responseBuffer = error.toString();
	client.bytesSent = 0;
	client.responseReady = true;
	client.state = WRITING;
}

static int checkCgiScriptPath(const std::string &scriptPath)
{
	struct stat scriptStat;

	if (scriptPath.empty())
		return (404);

	if (stat(scriptPath.c_str(), &scriptStat) == -1)
	{
		if (errno == ENOENT || errno == ENOTDIR)
			return (404);
		return (403);
	}

	if (S_ISDIR(scriptStat.st_mode))
		return (403);

	if (access(scriptPath.c_str(), R_OK) == -1)
		return (403);

	return (0);
}

static int checkCgiExecutablePath(const std::string &executablePath)
{
	struct stat executableStat;

	if (executablePath.empty())
		return (502);

	if (stat(executablePath.c_str(), &executableStat) == -1)
		return (502);

	if (S_ISDIR(executableStat.st_mode))
		return (502);

	if (access(executablePath.c_str(), X_OK) == -1)
		return (502);

	return (0);
}

static int validateCgiContext(const CgiContext &context)
{
	int statusCode;

	statusCode = checkCgiScriptPath(context.scriptPath);
	if (statusCode != 0)
		return (statusCode);

	statusCode = checkCgiExecutablePath(context.executable);
	if (statusCode != 0)
		return (statusCode);

	return (0);
}

int WebServ::startCgiForClient(Client &client, const RouteConfig &route)
{
	CgiContext context;
	CgiProcess process;
	const ServerConfig &server = configs[client.serverIndex];

	context = CgiRequestHandler::buildContext(client.request, route, server, client.getRemoteAddr());

	int validationStatus;

	validationStatus = validateCgiContext(context);
	if (validationStatus != 0)
	{
		prepareCgiErrorResponse(client, server, validationStatus);
		pollManager.setEvents(client.fd, POLLOUT);
		return (0);
	}

	if (CgiHandler::startCgi(context, process) != 0)
	{
		Response error = ErrorResponseHandler::build(502, "Bad Gateway", server);

		client.responseBuffer = error.toString();
		client.bytesSent = 0;
		client.responseReady = true;
		client.state = WRITING;
		pollManager.setEvents(client.fd, POLLOUT);
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
		pollManager.setEvents(client.fd, POLLOUT);
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

	cgiManager.registerCgiFd(client.cgiStdoutFd, client.fd);
	pollManager.addFd(client.cgiStdoutFd, POLLIN);

	if (client.cgiInputBuffer.empty())
	{
		close(client.cgiStdinFd);
		client.cgiStdinFd = -1;
		client.cgiStdinClosed = true;
		client.state = CGI_READING;
	}
	else
	{
		cgiManager.registerCgiFd(client.cgiStdinFd, client.fd);
		pollManager.addFd(client.cgiStdinFd, POLLOUT);
		client.state = CGI_WRITING;
	}

	pollManager.setEvents(client.fd, POLLIN);
	return (0);
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

		std::cout << "Sockets Ready - " << ready << "\n" << std::endl;

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
