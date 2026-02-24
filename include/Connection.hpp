#pragma once
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "Config.hpp"
#include <string>
#include <ctime>
#include <sys/types.h>

enum ConnectionState {
    CS_READING,
    CS_WRITING,
    CS_CLOSED
};

class Connection {
public:
    Connection(int fd, const ServerConfig& config);
    ~Connection();

    int getFd() const;
    ConnectionState getState() const;
    time_t getLastActivity() const;

    void onReadable();
    void onWritable();

    bool isClosed() const;
    void close();

    // CGI integration
    int cgiReadFd;
    int cgiWriteFd;
    pid_t cgiPid;
    std::string cgiWriteBuf;
    std::string cgiReadBuf;
    time_t cgiStartTime;

    void onCgiReadable();
    void onCgiWritable();
    void finishCgi();

private:
    int fd_;
    ConnectionState state_;
    time_t lastActivity_;
    const ServerConfig& config_;

    HttpRequest request_;
    std::string responseBuffer_;
    size_t bytesSent_;

    void buildResponse();
    void closeInternal();
};
