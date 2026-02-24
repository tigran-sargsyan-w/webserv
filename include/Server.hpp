#pragma once
#include "Config.hpp"

class Server {
public:
    Server(const ServerConfig& config);
    ~Server();

    int getListenFd() const;
    const ServerConfig& getConfig() const;

    void start();

private:
    const ServerConfig& config_;
    int listenFd_;
};
