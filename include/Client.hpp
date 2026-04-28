#ifndef CLIENT_HPP
# define CLIENT_HPP

# include "Request.hpp"

class Client 
{
  public:
    Client(int fd);
    ~Client() {};
    int getFd() const { return fd; };
    const Request& getRequest() const { return this->request; };
    std::string& getRawRequest() { return this->rawRequest; };

    void setFd(int fd) { this->fd = fd; };
    void setRequest(Request& request) { this->request = request; };
    int  fillInBuffer();
    int  fillOutBuffer();
    bool isRequestReady() { return this->requestReady; };
    bool isRequestValid() { return this->requestValid; };

  private:
    int fd;
    Request request;
    std::string rawRequest;
    bool requestValid;
    bool requestReady;
    //TODO: check if rawRequest is valid and finished before parsing
};

#endif
