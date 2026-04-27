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
    std::string& getRawRequest() { return _rawRequest; };

    void setFd(int fd) { _fd = fd; };
    void setRequest(Request& request) { _request = request; };
    int  fillInBuffer();
    int  fillOutBuffer();
    bool isRequestReady() { return _request.isReady(); };

  private:
    int _fd;
    Request _request;
    std::string _rawRequest;
};

#endif
