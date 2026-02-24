#include "EventLoop.hpp"
#include "Utils.hpp"
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <csignal>
#include <iostream>
#include <algorithm>

static volatile sig_atomic_t g_running = 1;
static void sigHandler(int) { g_running = 0; }

static const int CONN_TIMEOUT = 60;
static const int CGI_TIMEOUT  = 30;

EventLoop::EventLoop(const Config& config) : config_(config) {
    signal(SIGINT,  sigHandler);
    signal(SIGTERM, sigHandler);
    signal(SIGPIPE, SIG_IGN);

    for (size_t i = 0; i < config_.servers.size(); i++) {
        Server* srv = new Server(config_.servers[i]);
        srv->start();
        servers_.push_back(srv);
        std::cout << "Listening on " << config_.servers[i].host
                  << ":" << config_.servers[i].port << "\n";
    }
}

EventLoop::~EventLoop() {
    for (std::map<int, Connection*>::iterator it = connections_.begin();
         it != connections_.end(); ++it)
        delete it->second;
    for (size_t i = 0; i < servers_.size(); i++)
        delete servers_[i];
}

// ── poll fd builder ───────────────────────────────────────────────────────────

void EventLoop::buildPollFds(std::vector<pollfd>& fds) {
    fds.clear();

    // listen sockets
    for (size_t i = 0; i < servers_.size(); i++) {
        pollfd pfd;
        pfd.fd      = servers_[i]->getListenFd();
        pfd.events  = POLLIN;
        pfd.revents = 0;
        fds.push_back(pfd);
    }

    // client connections
    for (std::map<int, Connection*>::iterator it = connections_.begin();
         it != connections_.end(); ++it) {
        Connection* conn = it->second;
        if (conn->isClosed()) continue;

        pollfd pfd;
        pfd.fd      = conn->getFd();
        pfd.events  = 0;
        pfd.revents = 0;
        if (conn->getState() == CS_READING)  pfd.events |= POLLIN;
        if (conn->getState() == CS_WRITING)  pfd.events |= POLLOUT;
        fds.push_back(pfd);

        // CGI read pipe
        if (conn->cgiReadFd != -1) {
            pollfd cpfd;
            cpfd.fd      = conn->cgiReadFd;
            cpfd.events  = POLLIN;
            cpfd.revents = 0;
            fds.push_back(cpfd);
        }
        // CGI write pipe
        if (conn->cgiWriteFd != -1) {
            pollfd cpfd;
            cpfd.fd      = conn->cgiWriteFd;
            cpfd.events  = POLLOUT;
            cpfd.revents = 0;
            fds.push_back(cpfd);
        }
    }
}

// ── accept ────────────────────────────────────────────────────────────────────

void EventLoop::acceptConnections(int listenFd, const ServerConfig& serverConfig) {
    while (true) {
        struct sockaddr_in addr;
        socklen_t addrLen = sizeof(addr);
        int clientFd = accept(listenFd, (struct sockaddr*)&addr, &addrLen);
        if (clientFd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            break;
        }
        Connection* conn = new Connection(clientFd, serverConfig);
        connections_[clientFd] = conn;
    }
}

// ── remove connection ─────────────────────────────────────────────────────────

void EventLoop::removeConnection(int fd) {
    std::map<int, Connection*>::iterator it = connections_.find(fd);
    if (it == connections_.end()) return;
    Connection* conn = it->second;

    // remove any CGI fds from cgiFdToConn_
    if (conn->cgiReadFd  != -1) cgiFdToConn_.erase(conn->cgiReadFd);
    if (conn->cgiWriteFd != -1) cgiFdToConn_.erase(conn->cgiWriteFd);

    delete conn;
    connections_.erase(it);
}

// ── timeouts ──────────────────────────────────────────────────────────────────

void EventLoop::checkTimeouts() {
    time_t now = time(NULL);
    std::vector<int> toRemove;

    for (std::map<int, Connection*>::iterator it = connections_.begin();
         it != connections_.end(); ++it) {
        Connection* conn = it->second;
        if (conn->isClosed()) { toRemove.push_back(it->first); continue; }

        // CGI timeout
        if (conn->cgiPid > 0 && conn->cgiStartTime > 0) {
            if (now - conn->cgiStartTime > CGI_TIMEOUT) {
                conn->close();
                toRemove.push_back(it->first);
                continue;
            }
        }

        // connection timeout
        if (now - conn->getLastActivity() > CONN_TIMEOUT) {
            conn->close();
            toRemove.push_back(it->first);
        }
    }

    for (size_t i = 0; i < toRemove.size(); i++)
        removeConnection(toRemove[i]);
}

// ── process poll events ───────────────────────────────────────────────────────

void EventLoop::processPollEvents(const std::vector<pollfd>& fds) {
    size_t numServers = servers_.size();

    for (size_t i = 0; i < fds.size(); i++) {
        if (fds[i].revents == 0) continue;
        int fd = fds[i].fd;

        // listen socket?
        if (i < numServers) {
            if (fds[i].revents & POLLIN)
                acceptConnections(fd, servers_[i]->getConfig());
            continue;
        }

        // CGI fd?
        std::map<int, Connection*>::iterator cgit = cgiFdToConn_.find(fd);
        if (cgit != cgiFdToConn_.end()) {
            Connection* conn = cgit->second;
            if (fds[i].revents & (POLLIN | POLLHUP)) conn->onCgiReadable();
            if (fds[i].revents & POLLOUT)             conn->onCgiWritable();
            if (conn->isClosed()) removeConnection(conn->getFd());
            continue;
        }

        // client connection
        std::map<int, Connection*>::iterator cit = connections_.find(fd);
        if (cit == connections_.end()) continue;
        Connection* conn = cit->second;

        if (fds[i].revents & (POLLIN | POLLHUP | POLLERR))
            conn->onReadable();
        if (!conn->isClosed() && (fds[i].revents & POLLOUT))
            conn->onWritable();

        // register new CGI fds if any
        if (!conn->isClosed()) {
            if (conn->cgiReadFd  != -1) cgiFdToConn_[conn->cgiReadFd]  = conn;
            if (conn->cgiWriteFd != -1) cgiFdToConn_[conn->cgiWriteFd] = conn;
        }

        if (conn->isClosed()) removeConnection(fd);
    }
}

// ── main loop ─────────────────────────────────────────────────────────────────

void EventLoop::run() {
    std::vector<pollfd> fds;
    while (g_running) {
        buildPollFds(fds);
        if (fds.empty()) { usleep(10000); continue; }

        int ret = poll(&fds[0], (nfds_t)fds.size(), 5000);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (ret > 0) processPollEvents(fds);
        checkTimeouts();
    }
    std::cout << "Server shutting down.\n";
}
