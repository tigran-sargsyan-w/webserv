#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include "Client.hpp"
#include "Response.hpp"
#include "RequestParser.hpp"
#include "RequestHandler.hpp"

Client::Client(int fd) : _fd(fd) {}

int  Client::fillInBuffer()
{
      ssize_t bytes = recv(_fd, inBuffer, sizeof(inBuffer) - 1, 0);
      if (bytes == -1) {
        std::cout << "recv() failed\n";
        return (1);
      } else if (bytes == 0) {
        std::cout << "Client closed connection\n";
      } else {
        inBuffer[bytes] = '\0';
        std::cout << "Request from client:\n\n" << inBuffer << std::endl;
      }
      Request request = RequestParser::parse(std::string(inBuffer));
      setRequest(request);
      std::cout << "IS CGI TRUE? : " << _request.getIsCgi() << std::endl;
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
