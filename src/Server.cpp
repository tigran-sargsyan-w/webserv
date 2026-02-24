#include "Server.hpp"
#include "Utils.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdexcept>
#include <cstring>
#include <cerrno>

Server::Server(const ServerConfig& config) : config_(config), listenFd_(-1) {}

Server::~Server() {
    if (listenFd_ != -1) close(listenFd_);
}

int                 Server::getListenFd() const { return listenFd_; }
const ServerConfig& Server::getConfig()  const { return config_; }

void Server::start() {
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0)
        throw std::runtime_error(std::string("socket(): ") + strerror(errno));

    int opt = 1;
    setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    Utils::setNonBlocking(listenFd_);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons((uint16_t)config_.port);

    if (config_.host == "0.0.0.0" || config_.host.empty())
        addr.sin_addr.s_addr = INADDR_ANY;
    else
        inet_aton(config_.host.c_str(), &addr.sin_addr);

    if (bind(listenFd_, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        throw std::runtime_error(std::string("bind() port ") +
            Utils::intToStr(config_.port) + ": " + strerror(errno));

    if (listen(listenFd_, 128) < 0)
        throw std::runtime_error(std::string("listen(): ") + strerror(errno));
}
