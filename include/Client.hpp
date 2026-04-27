#ifndef CLIENT_HPP
# define CLIENT_HPP

# include "Request.hpp"

class Client 
{
  public:
    Client(int fd);
    ~Client() {};
    int getFd() const { return fd; };
    const Request& getRequest() const { return _request; };
    std::string& getRawRequest() { return _rawRequest; };

    void setFd(int fd) { this->fd = fd; };
    void setRequest(Request& request) { _request = request; };
    int  fillInBuffer();
    int  fillOutBuffer();
    bool isRequestReady() { return requestReady; };
    bool isRequestValid() { return requestValid; };

  private:
    int fd;
    Request _request;
    std::string _rawRequest;
    bool requestValid;
    bool requestReady;
    //TODO: check if rawRequest is valid and finished before parsing
};

#endif
