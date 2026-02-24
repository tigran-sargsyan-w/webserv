#pragma once
#include "Server.hpp"
#include "Connection.hpp"
#include "Config.hpp"
#include <vector>
#include <map>
#include <poll.h>

class EventLoop {
public:
    EventLoop(const Config& config);
    ~EventLoop();

    void run();

private:
    const Config& config_;
    std::vector<Server*> servers_;
    std::map<int, Connection*> connections_;   // client fd -> Connection
    std::map<int, Connection*> cgiFdToConn_;   // cgi fd   -> Connection

    void acceptConnections(int listenFd, const ServerConfig& serverConfig);
    void removeConnection(int fd);
    void checkTimeouts();
    void buildPollFds(std::vector<pollfd>& fds);
    void processPollEvents(const std::vector<pollfd>& fds);
};
