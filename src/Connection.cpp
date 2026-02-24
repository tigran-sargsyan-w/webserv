#include "Connection.hpp"
#include "Router.hpp"
#include "Utils.hpp"
#include <unistd.h>
#include <sys/socket.h>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <sys/wait.h>
#include <signal.h>

static const size_t READ_BUF_SIZE = 8192;
static const int    CONN_TIMEOUT  = 60; // seconds

Connection::Connection(int fd, const ServerConfig& config)
    : cgiReadFd(-1), cgiWriteFd(-1), cgiPid(-1), cgiStartTime(0),
      fd_(fd), state_(CS_READING), lastActivity_(time(NULL)),
      config_(config), bytesSent_(0) {
    request_.maxBodySize = config.clientMaxBodySize;
    Utils::setNonBlocking(fd_);
}

Connection::~Connection() {
    closeInternal();
}

int              Connection::getFd()           const { return fd_; }
ConnectionState  Connection::getState()        const { return state_; }
time_t           Connection::getLastActivity() const { return lastActivity_; }
bool             Connection::isClosed()        const { return state_ == CS_CLOSED; }

// ── readable ──────────────────────────────────────────────────────────────────

void Connection::onReadable() {
    if (state_ != CS_READING) return;
    lastActivity_ = time(NULL);

    char buf[READ_BUF_SIZE];
    ssize_t n = recv(fd_, buf, sizeof(buf), 0);
    if (n <= 0) {
        closeInternal();
        return;
    }

    int fed = request_.feed(buf, (int)n);
    if (fed < 0 || request_.isError()) {
        int errCode = request_.getErrorCode();
        if (errCode == 0) errCode = 400;
        // build error response directly
        HttpResponse resp;
        resp.setStatus(errCode);
        std::string body = "<html><body><h1>" +
            Utils::intToStr(errCode) + " " +
            HttpResponse::statusText(errCode) + "</h1></body></html>";
        resp.setBody(body, "text/html");
        resp.setHeader("Connection", "close");
        responseBuffer_ = resp.build();
        bytesSent_ = 0;
        state_ = CS_WRITING;
        return;
    }

    if (request_.isComplete()) {
        buildResponse();
    }
}

// ── writable ──────────────────────────────────────────────────────────────────

void Connection::onWritable() {
    if (state_ != CS_WRITING) return;
    lastActivity_ = time(NULL);

    size_t remaining = responseBuffer_.size() - bytesSent_;
    if (remaining == 0) { closeInternal(); return; }

    ssize_t n = send(fd_, responseBuffer_.c_str() + bytesSent_, remaining, 0);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return;
        closeInternal();
        return;
    }
    bytesSent_ += (size_t)n;
    if (bytesSent_ >= responseBuffer_.size()) {
        // check keep-alive
        std::map<std::string, std::string>::const_iterator it =
            request_.headers.find("connection");
        bool keepAlive = (request_.version == "HTTP/1.1");
        if (it != request_.headers.end())
            keepAlive = (Utils::toLower(it->second) == "keep-alive");

        if (keepAlive) {
            // reset for next request
            request_       = HttpRequest();
            request_.maxBodySize = config_.clientMaxBodySize;
            responseBuffer_.clear();
            bytesSent_ = 0;
            state_     = CS_READING;
        } else {
            closeInternal();
        }
    }
}

// ── CGI readable ─────────────────────────────────────────────────────────────

void Connection::onCgiReadable() {
    char buf[4096];
    ssize_t n = read(cgiReadFd, buf, sizeof(buf));
    if (n > 0) {
        cgiReadBuf.append(buf, (size_t)n);
    } else {
        ::close(cgiReadFd);
        cgiReadFd = -1;
        finishCgi();
    }
}

// ── CGI writable ─────────────────────────────────────────────────────────────

void Connection::onCgiWritable() {
    if (cgiWriteBuf.empty()) {
        ::close(cgiWriteFd);
        cgiWriteFd = -1;
        return;
    }
    ssize_t n = write(cgiWriteFd, cgiWriteBuf.c_str(), cgiWriteBuf.size());
    if (n > 0) cgiWriteBuf.erase(0, (size_t)n);
    if (cgiWriteBuf.empty()) { ::close(cgiWriteFd); cgiWriteFd = -1; }
}

// ── finish CGI ────────────────────────────────────────────────────────────────

void Connection::finishCgi() {
    if (cgiPid > 0) {
        waitpid(cgiPid, NULL, WNOHANG);
        cgiPid = -1;
    }

    HttpResponse resp;
    resp.setStatus(200);
    const std::string& out = cgiReadBuf;

    // find header/body separator
    size_t headerEnd = out.find("\r\n\r\n");
    size_t bodyStart = std::string::npos;
    if (headerEnd != std::string::npos) {
        bodyStart = headerEnd + 4;
    } else {
        headerEnd = out.find("\n\n");
        if (headerEnd != std::string::npos)
            bodyStart = headerEnd + 2;
    }

    if (bodyStart == std::string::npos) {
        // no header separator – treat everything as body
        resp.setBody(out, "text/html");
    } else {
        std::string headerPart = out.substr(0, headerEnd);
        std::string body       = out.substr(bodyStart);

        std::string::size_type pos = 0;
        while (pos < headerPart.size()) {
            std::string::size_type nl = headerPart.find('\n', pos);
            if (nl == std::string::npos) nl = headerPart.size();
            std::string line = Utils::trim(headerPart.substr(pos, nl - pos));
            pos = nl + 1;
            if (line.empty()) continue;
            if (line.size() >= 7 && line.substr(0, 7) == "Status:") {
                resp.setStatus((int)Utils::strToLong(Utils::trim(line.substr(7))));
            } else {
                size_t colon = line.find(':');
                if (colon != std::string::npos)
                    resp.setHeader(Utils::trim(line.substr(0, colon)),
                                   Utils::trim(line.substr(colon + 1)));
            }
        }
        resp.setBody(body);
        if (resp.headers.find("Content-Type") == resp.headers.end())
            resp.setHeader("Content-Type", "text/html");
    }

    resp.setHeader("Connection", "close");
    responseBuffer_ = resp.build();
    bytesSent_ = 0;
    state_ = CS_WRITING;
}

// ── build response ────────────────────────────────────────────────────────────

void Connection::buildResponse() {
    Router router(config_);
    HttpResponse resp = router.route(request_);
    resp.setHeader("Server", "webserv/1.0");

    // Connection header
    std::map<std::string, std::string>::const_iterator it =
        request_.headers.find("connection");
    bool keepAlive = (request_.version == "HTTP/1.1");
    if (it != request_.headers.end())
        keepAlive = (Utils::toLower(it->second) == "keep-alive");
    resp.setHeader("Connection", keepAlive ? "keep-alive" : "close");

    responseBuffer_ = resp.build();
    bytesSent_ = 0;
    state_ = CS_WRITING;
}

// ── close ─────────────────────────────────────────────────────────────────────

void Connection::close() { closeInternal(); }

void Connection::closeInternal() {
    if (state_ == CS_CLOSED) return;
    state_ = CS_CLOSED;
    if (fd_ != -1) { ::close(fd_); fd_ = -1; }
    if (cgiReadFd  != -1) { ::close(cgiReadFd);  cgiReadFd  = -1; }
    if (cgiWriteFd != -1) { ::close(cgiWriteFd); cgiWriteFd = -1; }
    if (cgiPid > 0) { kill(cgiPid, SIGKILL); waitpid(cgiPid, NULL, WNOHANG); cgiPid = -1; }
}
