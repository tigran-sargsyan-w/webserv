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
    void setIsRequestReady() { _requestReady = true; };
    int  fillInBuffer();
    int  fillOutBuffer();
    bool isRequestReady() { return _requestReady; };

  private:
    int _fd;
    Request _request;
    bool _requestReady;
    std::string _rawRequest;
};

#endif
