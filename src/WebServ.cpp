#include "WebServ.hpp"
#include "Request.hpp"
#include "RequestParser.hpp"
#include "Response.hpp"
#include "RequestHandler.hpp"
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

int  WebServ::readFromClient(Client& client)
{
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
        //std::cout << "Request from client:\n\n" << client.rawRequest << std::endl;
      }

      return (0);
}

int WebServ::SendToClient(Client& client)
{
    Response response = RequestHandler::handleRequest(client.request);
    std::cout << "Response to client:\n\n" << response.toString() << std::endl;
    
    ssize_t bytesSent = send(client.fd, response.toString().c_str(),
         response.toString().size(), 0);
    if (bytesSent == -1)
    {
      if (errno == EWOULDBLOCK || EAGAIN)
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
      Client& curClient = _clients.at(curFD);
      if (_pollfds[i].revents & POLLIN)
      {
        curClient.state = READING;
        this->readFromClient(curClient);
        if (curClient.state == CLOSING_CONNECTION)
        {
          close(curFD);
          removePollfd(curFD);
          _clients.erase(curFD);
        }

        RequestParser parser;
        if (parser.checkRequest(curClient.getRawRequest()) == COMPLETED)
        {
          parser.parse(curClient.getRawRequest(), curClient.request);
          std::cout << "METHOD: "  << curClient.request.getMethod() << "\n\n";
        }
        else 
          continue;

        //TODO: if request is valid set as POLLOUT
        _pollfds[i].events = POLLOUT;
      }
      if (_pollfds[i].revents & POLLOUT)
      {
        curClient.state = WRITING;
        this->SendToClient(curClient);
        if (curClient.state == CLOSING_CONNECTION)
        {
          close(curFD);
          removePollfd(curFD);
          _clients.erase(curFD);
        }
      }
    }
    
  }
}
