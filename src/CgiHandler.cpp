#include "CgiHandler.hpp"
#include "Utils.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <poll.h>
#include <sys/resource.h>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <sstream>

static const int CGI_TIMEOUT = 30; // seconds

CgiHandler::CgiHandler(const HttpRequest& req, const std::string& scriptPath,
                        const ServerConfig& serverConfig)
    : req_(req), scriptPath_(scriptPath), serverConfig_(serverConfig), pid_(-1) {
    pipeIn_[0]  = pipeIn_[1]  = -1;
    pipeOut_[0] = pipeOut_[1] = -1;
}

CgiHandler::~CgiHandler() {
    cleanup();
}

void CgiHandler::cleanup() {
    if (pipeIn_[0]  != -1) { close(pipeIn_[0]);  pipeIn_[0]  = -1; }
    if (pipeIn_[1]  != -1) { close(pipeIn_[1]);  pipeIn_[1]  = -1; }
    if (pipeOut_[0] != -1) { close(pipeOut_[0]); pipeOut_[0] = -1; }
    if (pipeOut_[1] != -1) { close(pipeOut_[1]); pipeOut_[1] = -1; }
}

// ── env builder ───────────────────────────────────────────────────────────────

std::vector<std::string> CgiHandler::buildEnv() {
    std::vector<std::string> env;

    env.push_back("REDIRECT_STATUS=200");
    env.push_back("GATEWAY_INTERFACE=CGI/1.1");
    env.push_back("SERVER_PROTOCOL=HTTP/1.1");
    env.push_back("SERVER_SOFTWARE=webserv/1.0");
    env.push_back("REQUEST_METHOD=" + req_.method);
    env.push_back("SCRIPT_FILENAME=" + scriptPath_);
    env.push_back("SCRIPT_NAME=" + req_.path);
    env.push_back("PATH_INFO=" + req_.path);
    env.push_back("QUERY_STRING=" + req_.queryString);
    env.push_back("SERVER_NAME=" +
        (serverConfig_.serverNames.empty() ? "localhost" : serverConfig_.serverNames[0]));
    env.push_back("SERVER_PORT=" + Utils::intToStr(serverConfig_.port));

    std::map<std::string, std::string>::const_iterator it;
    it = req_.headers.find("content-type");
    if (it != req_.headers.end())
        env.push_back("CONTENT_TYPE=" + it->second);
    it = req_.headers.find("content-length");
    if (it != req_.headers.end())
        env.push_back("CONTENT_LENGTH=" + it->second);
    else if (!req_.body.empty())
        env.push_back("CONTENT_LENGTH=" + Utils::intToStr((long long)req_.body.size()));

    // pass HTTP_* headers
    for (it = req_.headers.begin(); it != req_.headers.end(); ++it) {
        std::string name = "HTTP_" + Utils::toUpper(it->first);
        for (size_t i = 5; i < name.size(); i++)
            if (name[i] == '-') name[i] = '_';
        env.push_back(name + "=" + it->second);
    }
    return env;
}

// ── main execute ─────────────────────────────────────────────────────────────

HttpResponse CgiHandler::execute() {
    if (pipe(pipeIn_) != 0 || pipe(pipeOut_) != 0) {
        cleanup();
        HttpResponse r; r.setStatus(500);
        r.setBody("CGI pipe error", "text/plain");
        return r;
    }

    std::vector<std::string> envVec = buildEnv();
    std::vector<char*> envp;
    for (size_t i = 0; i < envVec.size(); i++)
        envp.push_back(const_cast<char*>(envVec[i].c_str()));
    envp.push_back(NULL);

    // determine interpreter
    std::string ext = Utils::getExtension(scriptPath_);
    std::string interpreter;
    if (ext == ".py")  interpreter = "/usr/bin/python3";
    else if (ext == ".pl") interpreter = "/usr/bin/perl";
    else if (ext == ".sh") interpreter = "/bin/sh";
    // else direct exec

    // working directory = script's directory
    std::string workDir = scriptPath_;
    size_t slash = workDir.rfind('/');
    if (slash != std::string::npos) workDir = workDir.substr(0, slash);
    else workDir = ".";

    pid_ = fork();
    if (pid_ < 0) {
        cleanup();
        HttpResponse r; r.setStatus(500);
        r.setBody("CGI fork error", "text/plain");
        return r;
    }

    if (pid_ == 0) {
        // child
        dup2(pipeIn_[0],  STDIN_FILENO);
        dup2(pipeOut_[1], STDOUT_FILENO);
        close(pipeIn_[0]); close(pipeIn_[1]);
        close(pipeOut_[0]); close(pipeOut_[1]);

        // close all other fds
        {
            struct rlimit rl;
            int maxFd = 1024;
            if (getrlimit(RLIMIT_NOFILE, &rl) == 0 && rl.rlim_cur != RLIM_INFINITY)
                maxFd = (int)rl.rlim_cur;
            for (int fd = 3; fd < maxFd; fd++) close(fd);
        }

        chdir(workDir.c_str());

        if (!interpreter.empty()) {
            char* argv[3];
            argv[0] = const_cast<char*>(interpreter.c_str());
            argv[1] = const_cast<char*>(scriptPath_.c_str());
            argv[2] = NULL;
            execve(interpreter.c_str(), argv, &envp[0]);
        } else {
            char* argv[2];
            argv[0] = const_cast<char*>(scriptPath_.c_str());
            argv[1] = NULL;
            execve(scriptPath_.c_str(), argv, &envp[0]);
        }
        _exit(1);
    }

    // parent: close unused ends
    close(pipeIn_[0]);  pipeIn_[0]  = -1;
    close(pipeOut_[1]); pipeOut_[1] = -1;

    // make pipes non-blocking
    Utils::setNonBlocking(pipeIn_[1]);
    Utils::setNonBlocking(pipeOut_[0]);

    // write body to CGI stdin, read output via poll
    const std::string& body = req_.body;
    size_t written = 0;
    std::string output;

    time_t startTime = time(NULL);

    while (true) {
        if (time(NULL) - startTime > CGI_TIMEOUT) {
            kill(pid_, SIGTERM);
            usleep(100000); // 100ms grace period
            if (waitpid(pid_, NULL, WNOHANG) == 0)
                kill(pid_, SIGKILL);
            break;
        }

        struct pollfd pfds[2];
        int nfds = 0;

        if (pipeIn_[1] != -1 && written < body.size()) {
            pfds[nfds].fd     = pipeIn_[1];
            pfds[nfds].events = POLLOUT;
            pfds[nfds].revents = 0;
            nfds++;
        } else if (pipeIn_[1] != -1) {
            close(pipeIn_[1]); pipeIn_[1] = -1;
        }

        int readIdx = -1;
        if (pipeOut_[0] != -1) {
            readIdx = nfds;
            pfds[nfds].fd     = pipeOut_[0];
            pfds[nfds].events = POLLIN;
            pfds[nfds].revents = 0;
            nfds++;
        }

        if (nfds == 0) break;

        int ret = poll(pfds, (nfds_t)nfds, 5000);
        if (ret <= 0) break;

        // write
        for (int i = 0; i < nfds; i++) {
            if (pfds[i].fd == pipeIn_[1] && (pfds[i].revents & POLLOUT)) {
                ssize_t w = write(pipeIn_[1], body.c_str() + written,
                                  body.size() - written);
                if (w > 0) written += (size_t)w;
                if (written >= body.size()) {
                    close(pipeIn_[1]); pipeIn_[1] = -1;
                }
            }
        }

        // read
        if (readIdx >= 0 && (pfds[readIdx].revents & (POLLIN | POLLHUP))) {
            char buf[4096];
            ssize_t r = read(pipeOut_[0], buf, sizeof(buf));
            if (r > 0) output.append(buf, (size_t)r);
            else if (r == 0) { close(pipeOut_[0]); pipeOut_[0] = -1; }
            else if (errno != EAGAIN) { close(pipeOut_[0]); pipeOut_[0] = -1; }
        }
    }

    // drain any remaining output
    if (pipeOut_[0] != -1) {
        char buf[4096];
        ssize_t r;
        while ((r = read(pipeOut_[0], buf, sizeof(buf))) > 0)
            output.append(buf, (size_t)r);
        close(pipeOut_[0]); pipeOut_[0] = -1;
    }

    int status;
    waitpid(pid_, &status, 0);
    pid_ = -1;
    cleanup();

    return parseCgiOutput(output);
}

// ── CGI output parser ─────────────────────────────────────────────────────────

HttpResponse CgiHandler::parseCgiOutput(const std::string& output) {
    HttpResponse resp;
    resp.setStatus(200);

    size_t headerEnd = output.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        headerEnd = output.find("\n\n");
        if (headerEnd == std::string::npos) {
            // no headers - treat entire output as body
            resp.setBody(output, "text/html");
            return resp;
        }
        // \n\n style
        std::string headerPart = output.substr(0, headerEnd);
        std::string body       = output.substr(headerEnd + 2);

        std::istringstream iss(headerPart);
        std::string line;
        while (std::getline(iss, line)) {
            line = Utils::trim(line);
            if (line.empty()) continue;
            if (line.substr(0, 7) == "Status:") {
                int code = (int)Utils::strToLong(Utils::trim(line.substr(7)));
                resp.setStatus(code);
            } else {
                size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    std::string name  = Utils::trim(line.substr(0, colon));
                    std::string value = Utils::trim(line.substr(colon + 1));
                    resp.setHeader(name, value);
                }
            }
        }
        resp.setBody(body);
        if (resp.headers.find("Content-Type") == resp.headers.end())
            resp.setHeader("Content-Type", "text/html");
        return resp;
    }

    // \r\n\r\n style
    std::string headerPart = output.substr(0, headerEnd);
    std::string body       = output.substr(headerEnd + 4);

    // parse CGI headers
    size_t pos = 0;
    while (pos < headerPart.size()) {
        size_t nl = headerPart.find("\r\n", pos);
        if (nl == std::string::npos) nl = headerPart.size();
        std::string line = Utils::trim(headerPart.substr(pos, nl - pos));
        pos = nl + 2;
        if (line.empty()) continue;
        if (line.size() >= 7 && line.substr(0, 7) == "Status:") {
            int code = (int)Utils::strToLong(Utils::trim(line.substr(7)));
            resp.setStatus(code);
        } else {
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string name  = Utils::trim(line.substr(0, colon));
                std::string value = Utils::trim(line.substr(colon + 1));
                resp.setHeader(name, value);
            }
        }
    }

    resp.setBody(body);
    if (resp.headers.find("Content-Type") == resp.headers.end())
        resp.setHeader("Content-Type", "text/html");

    return resp;
}
