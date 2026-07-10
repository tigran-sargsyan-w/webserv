#ifndef CLIENT_HPP
#define CLIENT_HPP
#include "Request.hpp"
#include "CgiSession.hpp"
#include <ctime>
#include <cstddef>

enum ClientState
{
  WRITING,
  READING,
  DISCARDING_BODY,
  CGI_WRITING,
  CGI_READING,
  CLOSING_CONNECTION
};

class Client
{
public:
  Client(int fd);
  ~Client() {};
  int getFd() const { return fd; };
  const Request &getRequest() const { return this->request; };
  std::string &getRawRequest() { return this->rawRequest; };
  const std::string &getRemoteAddr() const { return (this->remoteAddr); };
  void setRemoteAddr(const std::string &remoteAddr) { this->remoteAddr = remoteAddr; };
  void touchActivity();

  int fd;
  Request request;
  std::string rawRequest;
  ClientState state;
  size_t serverIndex;
  std::string responseBuffer;
  size_t bytesSent;
  size_t bodyBytesToDiscard;
  bool responseReady;
  CgiSession cgi;
  time_t lastActivity;
private:
  std::string remoteAddr;
};

#endif
