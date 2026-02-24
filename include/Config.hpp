#pragma once
#include <string>
#include <vector>
#include <map>

struct LocationConfig {
    std::string path;
    std::string root;
    std::string index;
    std::vector<std::string> allowedMethods;
    bool autoindex;
    std::string uploadDir;
    std::string cgiExtension;
    long long clientMaxBodySize;
    std::string redirect;

    LocationConfig();
};

struct ServerConfig {
    std::string host;
    int port;
    std::vector<std::string> serverNames;
    long long clientMaxBodySize;
    std::map<int, std::string> errorPages;
    std::vector<LocationConfig> locations;

    ServerConfig();
};

struct Config {
    std::vector<ServerConfig> servers;
};
