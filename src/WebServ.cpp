#include "WebServ.hpp"
#include "Request.hpp"
#include "RequestParser.hpp"
#include <fcntl.h>
#include <cstring>
#include <cerrno>
#include <iostream>
#include <poll.h>
#include <sys/poll.h>
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

int WebServ::initListeningSocket() {
  this->serverSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (this->serverSocket == -1) {
    std::cerr << "socket() failed: " << std::strerror(errno) << "\n";
    return (1);
  }

  std::cout << "Server socked created, FD = " << this->serverSocket << "\n";

  int opt = 1;
  if (setsockopt(this->serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) ==
      -1) {
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

  sockaddr_in addr;

  std::memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(8080);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(this->serverSocket, (sockaddr *)&addr, sizeof(addr)) == -1) {
    std::cerr << "Error binding socket\n" << std::strerror(errno) << "\n";
    close(this->serverSocket);
    return (1);
  }
  return (0);
}

int WebServ::setup(const ServerConfig &serverConfig) {
  std::cout << "WebServ setup called!\n";

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
  fcntl(this->serverSocket, O_NONBLOCK);

	std::cout << "Listening on " << serverConfig.listen.host << ":" << serverConfig.listen.port << "\n";
  return (0);
}

int WebServ::acceptConnection() {

  int clientSocket = accept(this->serverSocket, NULL, NULL);
  if (clientSocket == -1)
    return (-1);
  fcntl(clientSocket, O_NONBLOCK);
  struct pollfd tmpPollfd;
  tmpPollfd.fd = clientSocket;
  tmpPollfd.events = POLLIN;
  _pollfds.push_back(tmpPollfd);
  _clients.insert(std::make_pair(clientSocket, Client(clientSocket)));
  return (clientSocket);
}


void  WebServ::removePollfd(int fd)
{
  for (size_t i = 0; i < _pollfds.size(); i++)
  {
    if (_pollfds[i].fd == fd)
    {
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

    if (_pollfds.at(0).revents == POLLIN)
    {
      int clientSocket;
      if ((clientSocket = acceptConnection()) == -1)
      {
        if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
          continue;
        std::cerr << "Error accepting client connection\n";
        close(this->serverSocket);
        return (1);
      }
      fcntl(clientSocket, O_NONBLOCK);
    }

    // 5. Receive data from client

    for (size_t i = 1; i < _pollfds.size(); i++)
    {
      int curFD = _pollfds[i].fd;
      if (_pollfds[i].revents & POLLIN)
      {
          Client& curClient = _clients.at(curFD);
          curClient.fillInBuffer();

        Request request = RequestParser::parse(curClient.getRawRequest());
        curClient.setRequest(request);

        if (curClient.getRequest().getMethod().empty()) {
          std::cout << "Failed to parse request\n";
          close(curFD);
          break;
        }
          if (curClient.getRequest().getMethod() != "GET") {
            std::cout << "Unsupported HTTP method: " << curClient.getRequest().getMethod() << "\n";
            close(curFD);
            break;
          }
          _pollfds[i].events = POLLOUT;
      }
      if (_pollfds[i].revents & POLLOUT)
      {
        Client& curClient = _clients.at(curFD);
        if (curClient.isRequestReady())
          curClient.fillOutBuffer();
        close(curFD);
        removePollfd(curFD);
        _clients.erase(curFD);
      }
    }
    
  }
}
