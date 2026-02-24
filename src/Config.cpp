#include "Config.hpp"

LocationConfig::LocationConfig()
    : autoindex(false), clientMaxBodySize(-1) {}

ServerConfig::ServerConfig()
    : host("0.0.0.0"), port(80), clientMaxBodySize(10LL * 1024 * 1024) {}
