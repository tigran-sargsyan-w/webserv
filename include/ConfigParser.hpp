#pragma once
#include "Config.hpp"
#include <string>
#include <vector>

class ConfigParser {
public:
    Config parse(const std::string& filename);

private:
    std::vector<std::string> tokens_;
    size_t pos_;

    void tokenize(const std::string& content);
    std::string next();
    std::string peek();
    bool hasMore();
    void expect(const std::string& tok);

    ServerConfig parseServer();
    LocationConfig parseLocation(const std::string& path);
    long long parseSize(const std::string& s);
};
