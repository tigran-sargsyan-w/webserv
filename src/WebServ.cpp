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

WebServ::WebServ()
{
	std::cout << "WebServ created!\n";
}

WebServ::WebServ(const WebServ& other)
{
	(void)other;
	std::cout << "WebServ copy constructor called!\n";
}

WebServ::~WebServ()
{
	std::cout << "WebServ destroyed!\n";
}

WebServ& WebServ::operator=(const WebServ& other)
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

int WebServ::readFromClient(Client& client)
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

static bool routeMatchesPath(const std::string& routePath,
                             const std::string& requestPath)
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

static const RouteConfig& findMatchingRoute(const ServerConfig& serverConfig,
                                            const std::string& requestPath)
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

static std::string getPathWithoutQuery(const std::string& path)
{
	size_t questionMark = path.find('?');

	if (questionMark == std::string::npos)
		return (path);
	return (path.substr(0, questionMark));
}

int WebServ::SendToClient(Client& client)
{
	std::string cleanPath = getPathWithoutQuery(client.request.getPath());
  std::cout << "TEST\n";
	const RouteConfig& route = findMatchingRoute(configs[client.serverIndex], cleanPath);
	std::cout << "Matched route: " << route.path << std::endl;
	Response response = RequestHandler::handleRequest(
	    client.request, route, configs[client.serverIndex], client.getRemoteAddr());
	std::cout << "Response to client:\n\n" << response.toString() << std::endl;

	ssize_t bytesSent = send(
	    client.fd, response.toString().c_str(), response.toString().size(), 0);
	if (bytesSent == -1)
	{
		if (errno == EWOULDBLOCK || errno == EAGAIN)
			return (0);
		std::cout << "send: " << strerror(errno) << std::endl;
		return (1);
	}
	if (bytesSent)
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

	struct pollfd tmpPollfd;
	tmpPollfd.fd = tmpSocket;
	tmpPollfd.events = POLLIN;
	tmpPollfd.revents = 0;
	this->pollFds.push_back(tmpPollfd);
	return (tmpSocket);
}

int WebServ::bindSockAddress (int listeningSocket, size_t configIndex)
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
  //TODO: add created ListenerSockets to pollfds and to map
	std::cout << "WebServ setup called!\n";

  this->configs = servers; // Refactor instead of copying?
  bool stop = true;
  for (size_t i = 0; i < servers.size(); ++i)
  {
    ServerConfig& tmpConfig = servers[i];

    // 1. Create socket
    int listeningSocket = initListeningSocket();
    if (listeningSocket == -1)
    {
      std::cerr << "Server Block " << i << " setup failed!\n";
      continue;
    }
    listenerFdToIndex[listeningSocket] = i;


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
      continue;
    }

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
	clientSocket = accept(
	    listeningSocket, (sockaddr *)&clientAddress, &clientAddressLength);
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

		clientIt->second.setRemoteAddr(
		    ipToString(clientAddress.sin_addr.s_addr));
    clients.at(clientSocket).serverIndex = listenerFdToIndex[listeningSocket];
  }
	return (clientSocket);
}

void WebServ::removePollfd(int fd)
{
	for (size_t i = 0; i < this->pollFds.size(); i++)
	{
		if (this->pollFds[i].fd == fd)
		{
			this->pollFds.erase(this->pollFds.begin() + i);
		}
	}
}

bool WebServ::isListeningFd(int fd)
{
  for (size_t i = 0; i < this->listenerFdToIndex.size(); i++)
  {
    if (this->listenerFdToIndex.find(fd) != this->listenerFdToIndex.end())
      return (true);
  }
  return (false);
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


		// 5. Receive data from client

		for (size_t i = 0; i < this->pollFds.size(); i++)
		{
			int curFD = this->pollFds[i].fd;

		// 4. Accept connections

      if (isListeningFd(curFD))
      {
        if (this->pollFds[i].revents & POLLIN)
          acceptConnection(curFD); //TODO: pass correct sock fd
      }
      else
      {
        
          if (this->pollFds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
          {
            close(curFD);
            removePollfd(curFD);
            this->clients.erase(curFD);
            i--;
            continue;
          }

          Client& curClient = clients.at(curFD);
          if (this->pollFds[i].revents & POLLIN)
          {
            curClient.state = READING;
            this->readFromClient(curClient);
            if (curClient.state == CLOSING_CONNECTION)
            {
              close(curFD);
              removePollfd(curFD);
              clients.erase(curFD);
              i--;
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
              continue;
            else
            {
              // TODO: RequestHandler for errors and close connection
              close(curFD);
              removePollfd(curFD);
              clients.erase(curFD);
              i--;
              continue;
            }

            // TODO: if request is valid set as POLLOUT
            this->pollFds[i].events = POLLOUT;
          }
          if (this->pollFds[i].revents & POLLOUT)
          {
            curClient.state = WRITING;
            this->SendToClient(curClient);
            if (curClient.state == CLOSING_CONNECTION)
            {
              close(curFD);
              removePollfd(curFD);
              clients.erase(curFD);
              i--;
              continue;
          }
        }
      }
		}
	}
}
