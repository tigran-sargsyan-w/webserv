#pragma once
#include "Config.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include <string>

class Router {
public:
    Router(const ServerConfig& config);

    HttpResponse route(const HttpRequest& req);
    const LocationConfig* findLocation(const std::string& path) const;

private:
    const ServerConfig& config_;

    HttpResponse handleGet(const HttpRequest& req, const LocationConfig& loc);
    HttpResponse handlePost(const HttpRequest& req, const LocationConfig& loc);
    HttpResponse handleDelete(const HttpRequest& req, const LocationConfig& loc);

    HttpResponse serveFile(const std::string& filePath, const std::string& urlPath);
    HttpResponse serveDirectory(const std::string& dirPath, const std::string& urlPath,
                                const LocationConfig& loc);
    HttpResponse generateAutoindex(const std::string& dirPath, const std::string& urlPath);

    HttpResponse errorResponse(int code);

    std::string resolvePath(const std::string& root, const std::string& reqPath,
                            const std::string& locPath);
    bool isMethodAllowed(const std::string& method, const LocationConfig& loc) const;
    bool isCgiRequest(const std::string& path, const LocationConfig& loc) const;
    HttpResponse handleCgi(const HttpRequest& req, const std::string& scriptPath,
                           const LocationConfig& loc);
};
