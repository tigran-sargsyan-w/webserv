#ifndef WEBSERV_HPP
#define WEBSERV_HPP

#include "CgiManager.hpp"
#include "Client.hpp"
#include "Config.hpp"
#include "ListenerSocketHandler.hpp"
#include "PollManager.hpp"
#include "ConnectionManager.hpp"
#include <map>
#include <vector>

class WebServ
{
public:
	WebServ();
	WebServ(const WebServ &other);
	~WebServ();
	WebServ &operator=(const WebServ &other);

	int setup(std::vector<ServerConfig> servers);
	int run();

private:
	PollManager pollManager;
	CgiManager cgiManager;
	ConnectionManager connectionManager;
	ListenerSocketHandler listenerSocketHandler;

	std::vector<ServerConfig> configs;
};

#endif
