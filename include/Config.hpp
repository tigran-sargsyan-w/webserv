#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <vector>

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
	std::set<std::string> methods;
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


#endif