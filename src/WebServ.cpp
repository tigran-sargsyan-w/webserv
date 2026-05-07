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

WebServ::WebServ() { std::cout << "WebServ created!\n"; }

WebServ::WebServ(const WebServ &other) {
  (void)other;
  std::cout << "WebServ copy constructor called!\n";
}

WebServ::~WebServ() {
  std::cout << "WebServ destroyed!\n";
  close(this->serverSocket);
}

WebServ &WebServ::operator=(const WebServ &other) {
  (void)other;
  std::cout << "WebServ assignement operator called!\n";
  return (*this);
}

int WebServ::setNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    std::cerr << "fcntl: " << strerror(errno) << "\n";
    return (1);
  }
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK)) {
    std::cerr << "fcntl: " << strerror(errno) << "\n";
    return (1);
  }
  return (0);
}

int WebServ::readFromClient(Client &client) {
  ssize_t bytesRead;
  char buffer[4096];

  bytesRead = recv(client.fd, buffer, sizeof(buffer) - 1, 0);
  if (bytesRead == -1) {
    if (errno == EWOULDBLOCK || errno == EAGAIN)
      return (0);
    return 1;
  } else if (bytesRead == 0) {
    std::cout << "Client closed connection\n";
    client.state = CLOSING_CONNECTION;
  } else {
    buffer[bytesRead] = '\0';
    client.rawRequest.append(buffer);
    // std::cout << "Request from client:\n\n" << client.rawRequest <<
    // std::endl;
  }

  return (0);
}

static bool routeMatchesPath(const std::string &routePath,
                             const std::string &requestPath) {
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

static const RouteConfig &findMatchingRoute(const ServerConfig &serverConfig,
                                            const std::string &requestPath) {
  const RouteConfig *bestRoute = NULL;
  size_t bestLength = 0;

  for (std::vector<RouteConfig>::const_iterator it =
           serverConfig.routes.begin();
       it != serverConfig.routes.end(); ++it) {
    if (routeMatchesPath(it->path, requestPath) &&
        it->path.length() > bestLength) {
      bestRoute = &(*it);
      bestLength = it->path.length();
    }
  }

  if (bestRoute != NULL)
    return (*bestRoute);

  return (serverConfig.routes.front());
}

static std::string getPathWithoutQuery(const std::string &path) {
  size_t questionMark = path.find('?');

  if (questionMark == std::string::npos)
    return (path);
  return (path.substr(0, questionMark));
}

int WebServ::SendToClient(Client &client) {
  std::string cleanPath = getPathWithoutQuery(client.request.getPath());
  const RouteConfig &route = findMatchingRoute(this->serverConfig, cleanPath);
  std::cout << "Matched route: " << route.path << std::endl;
  Response response = RequestHandler::handleRequest(
      client.request, route, this->serverConfig, client.getRemoteAddr());
  std::cout << "Response to client:\n\n" << response.toString() << std::endl;

  ssize_t bytesSent = send(client.fd, response.toString().c_str(),
                           response.toString().size(), 0);
  if (bytesSent == -1) {
    if (errno == EWOULDBLOCK || errno == EAGAIN)
      return (0);
    std::cout << "send: " << strerror(errno) << std::endl;
    return (1);
  }
  if (bytesSent)
    client.state = CLOSING_CONNECTION;
  return (0);
}

int WebServ::initListeningSocket() {
  this->serverSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (this->serverSocket == -1) {
    std::cerr << "socket() failed: " << std::strerror(errno) << "\n";
    return (1);
  }

  std::cout << "Server socked created, FD = " << this->serverSocket << "\n";

  int opt = 1;
  if (setsockopt(this->serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt,
                 sizeof(opt)) == -1) {
    std::cerr << "setsockopt() failed: " << std::strerror(errno) << "\n";
    close(this->serverSocket);
    return (1);
  }

  struct pollfd tmpPollfd;
  tmpPollfd.fd = this->serverSocket;
  tmpPollfd.events = POLLIN;
  _pollfds.push_back(tmpPollfd);
  return (0);
}

int WebServ::bindSockAddress() {

  struct addrinfo hints;
  struct addrinfo *res = NULL;

  std::memset(&hints, 0, sizeof(hints));

  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = 0;

  std::stringstream ss;
  ss << this->serverConfig.listen.port;
  std::string port_str = ss.str();

  const char *host_cstr = NULL;
  if (!this->serverConfig.listen.host.empty()) {
    host_cstr = this->serverConfig.listen.host.c_str();
  }

  int ret = getaddrinfo(host_cstr, port_str.c_str(), &hints, &res);
  if (ret) {
    std::cerr << "getaddrinfo: " << gai_strerror(ret) << "\n";
    return (1);
  }

  if (bind(this->serverSocket, res->ai_addr, res->ai_addrlen) == -1) {
    std::cerr << "Error binding socket\n" << std::strerror(errno) << "\n";
    freeaddrinfo(res);
    close(this->serverSocket);
    return (1);
  }
  freeaddrinfo(res);
  return (0);
}

int WebServ::setup(const ServerConfig &serverConfig) {
  std::cout << "WebServ setup called!\n";

  this->serverConfig = serverConfig;
  // 1. Create socket
  if (initListeningSocket())
    return (1);

  // 2. Setup address for socket
  if (bindSockAddress())
    return (1);

  // 3. Socket listening

  if (listen(this->serverSocket, 10) == -1) {
    std::cerr << "Error on socket listening\n";
    close(this->serverSocket);
    return (1);
  }
  if (setNonBlocking(this->serverSocket)) {
    return (1);
  }

  std::cout << "Listening on " << serverConfig.listen.host << ":"
            << serverConfig.listen.port << "\n";
  return (0);
}

static std::string ipToString(uint32_t address) {
  std::ostringstream oss;
  uint32_t hostAddress;

  hostAddress = ntohl(address);
  oss << ((hostAddress >> 24) & 255) << "." << ((hostAddress >> 16) & 255)
      << "." << ((hostAddress >> 8) & 255) << "." << (hostAddress & 255);
  return (oss.str());
}

int WebServ::acceptConnection() {
  sockaddr_in clientAddress;
  socklen_t clientAddressLength;
  int clientSocket;
  struct pollfd tmpPollfd;
  std::map<int, Client>::iterator clientIt;

  clientAddressLength = sizeof(clientAddress);
  clientSocket = accept(this->serverSocket, (sockaddr *)&clientAddress,
                        &clientAddressLength);
  if (clientSocket == -1)
    return (-1);
  if (setNonBlocking(clientSocket))
    return (-1);

  tmpPollfd.fd = clientSocket;
  tmpPollfd.events = POLLIN;
  _pollfds.push_back(tmpPollfd);

  _clients.insert(std::make_pair(clientSocket, Client(clientSocket)));
  clientIt = _clients.find(clientSocket);
  if (clientIt != _clients.end())
    clientIt->second.setRemoteAddr(ipToString(clientAddress.sin_addr.s_addr));
  return (clientSocket);
}

void WebServ::removePollfd(int fd) {
  for (size_t i = 0; i < _pollfds.size(); i++) {
    if (_pollfds[i].fd == fd) {
      _pollfds.erase(_pollfds.begin() + i);
    }
  }
}

int WebServ::run() {
  std::cout << "WebServ run called!\n";

  while (true) {
    int ready = poll(_pollfds.data(), _pollfds.size(), -1);
    if (ready < 0)
      continue;
    std::cout << "Sockets Ready - " << ready << "\n" << std::endl;

    // 4. Accept connections

    if (_pollfds.at(0).revents == POLLIN) {
      int clientSocket;
      if ((clientSocket = acceptConnection()) == -1) {
        if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
          continue;
        std::cerr << "Error accepting client connection\n";
        close(this->serverSocket);
        return (1);
      }
    }

    // 5. Receive data from client

    for (size_t i = 1; i < _pollfds.size(); i++) {
      int curFD = _pollfds[i].fd;
      Client &curClient = _clients.at(curFD);
      if (_pollfds[i].revents & POLLIN) {
        curClient.state = READING;
        this->readFromClient(curClient);
        if (curClient.state == CLOSING_CONNECTION) {
          close(curFD);
          removePollfd(curFD);
          _clients.erase(curFD);
        }

        RequestParser parser;
        RequestInspector inspector;

        inspector.inspectRequest(curClient.getRawRequest());
        if (inspector.status == COMPLETED) {
          parser.parse(curClient.getRawRequest(), curClient.request);
        } else if (inspector.status == NEED_MORE_DATA)
          continue;
        else {
          // TODO: RequestHandler for errors and close connection
          close(curFD);
          removePollfd(curFD);
          _clients.erase(curFD);
          continue;
        }

        // TODO: if request is valid set as POLLOUT
        _pollfds[i].events = POLLOUT;
      }
      if (_pollfds[i].revents & POLLOUT) {
        curClient.state = WRITING;
        this->SendToClient(curClient);
        if (curClient.state == CLOSING_CONNECTION) {
          close(curFD);
          removePollfd(curFD);
          _clients.erase(curFD);
        }
      }
    }
  }
}
