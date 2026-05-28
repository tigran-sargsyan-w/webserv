#ifndef CLIENT_HPP
# define CLIENT_HPP
# include "Request.hpp"
# include <ctime>
# include <sys/types.h>

enum ClientState
{
  WRITING,
  READING,
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
    const Request& getRequest() const { return this->request; };
    std::string& getRawRequest() { return this->rawRequest; };
    const std::string &getRemoteAddr() const { return (this->remoteAddr); };
    void setRemoteAddr(const std::string &remoteAddr) {this->remoteAddr = remoteAddr;};

    void setFd(int fd) { this->fd = fd; };
    void setRequest(Request& request) { this->request = request; };
    bool isRequestReady() { return this->requestReady; };
    bool isRequestValid() { return this->requestValid; };

    int fd;
    Request request;
    std::string rawRequest;
    bool requestValid;
    bool requestReady;
    ClientState state;
    size_t serverIndex;
    std::string responseBuffer;
    size_t bytesSent;
    bool responseReady;
    pid_t cgiPid;
    int cgiStdinFd;
    int cgiStdoutFd;

    std::string cgiInputBuffer;
    size_t cgiInputSent;
    std::string cgiOutputBuffer;

    bool cgiStdinClosed;
    bool cgiStdoutClosed;
    bool cgiFinished;
    time_t cgiStartTime;
    //TODO: check if rawRequest is valid and finished before parsing
  private:
    std::string remoteAddr;
};

#endif
