#ifndef CLIENT_HPP
# define CLIENT_HPP

# include "Request.hpp"

class Client 
{
  public:
    Client(int fd);
    ~Client() {};
    int getFd() const { return _fd; };
    const Request& getRequest() const { return _request; };
    char *getInBuffer() { return inBuffer; }

    void setFd(int fd) { _fd = fd; };
    void setRequest(Request& request) { _request = request; };
    int  fillInBuffer();
    int  fillOutBuffer();

  private:
    int _fd;
    Request _request;
    char inBuffer[4096];
    /*char outBuffer[4096];*/
};

#endif
