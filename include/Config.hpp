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

#endif