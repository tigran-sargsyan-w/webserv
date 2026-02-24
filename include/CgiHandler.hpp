#pragma once
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "Config.hpp"
#include <string>
#include <vector>
#include <sys/types.h>

class CgiHandler {
public:
    CgiHandler(const HttpRequest& req, const std::string& scriptPath,
               const ServerConfig& serverConfig);
    ~CgiHandler();

    HttpResponse execute();

private:
    const HttpRequest& req_;
    std::string scriptPath_;
    const ServerConfig& serverConfig_;
    pid_t pid_;
    int pipeIn_[2];
    int pipeOut_[2];

    std::vector<std::string> buildEnv();
    HttpResponse parseCgiOutput(const std::string& output);
    void cleanup();
};
