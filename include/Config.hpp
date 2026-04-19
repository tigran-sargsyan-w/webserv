#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <vector>
# include "HttpMethod.hpp"

struct ListenConfig
{
	std::string host;
	int port;

	ListenConfig() : host(""), port(0) {}
};

struct CgiConfig
{
	std::string extension;
	std::string executable;
};

struct RouteConfig
{
	std::string path;
	std::set<HttpMethod> methods;
	std::string root;
	std::string index;
	bool autoindex;
	bool uploadEnable;
	std::string uploadStore;
	bool hasReturn;
	int returnCode;
	std::string returnPath;
	std::vector<CgiConfig> cgi;

	RouteConfig() : autoindex(false), uploadEnable(false), hasReturn(false), returnCode(0) {}
};

struct ServerConfig
{
	ListenConfig listen;
	std::string serverName;
	std::string root;
	std::string index;
	size_t clientMaxBodySize;
	std::map<int, std::string> errorPages;
	std::vector<RouteConfig> routes;

	ServerConfig() : clientMaxBodySize(0) {}
};

struct Config
{
	std::vector<ServerConfig> servers;
};

#endif