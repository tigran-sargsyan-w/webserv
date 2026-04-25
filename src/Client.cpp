#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include "Client.hpp"
#include "Response.hpp"
#include "RequestHandler.hpp"
#include <errno.h>

Client::Client(int fd) : _fd(fd), _requestReady(false) {}

int  Client::fillInBuffer()
{
      ssize_t bytesRead;
      char buffer[4096];

      bytesRead = recv(_fd, buffer, sizeof(buffer) - 1, 0);
      _rawRequest.append(buffer, bytesRead);
      if (bytesRead == -1) {
        if (errno == EWOULDBLOCK || errno == EAGAIN)
          return (0);
        return 1;
      } else if (bytesRead == 0) {
        std::cout << "Client closed connection\n";
      } else {
        buffer[bytesRead] = '\0';
        _rawRequest.append(buffer);
        std::cout << "Request from client:\n\n" << _rawRequest << std::endl;
      }

      return (0);
}

int Client::fillOutBuffer()
{
    Response response = RequestHandler::handleRequest(_request);
    std::cout << "Response to client:\n\n" << response.toString() << std::endl;
    send(_fd, response.toString().c_str(),
         response.toString().size(), 0);
    close(_fd);
    return (0);
}
