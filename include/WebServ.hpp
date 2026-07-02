#ifndef WEBSERV_HPP
#define WEBSERV_HPP

#include "CgiManager.hpp"
#include "Client.hpp"
#include "Config.hpp"
#include "ListenerSocketHandler.hpp"
#include "PollManager.hpp"
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
	ListenerSocketHandler listenerSocketHandler;

	std::vector<ServerConfig> configs;
	std::map<int, Client> clients;

};

#endif
