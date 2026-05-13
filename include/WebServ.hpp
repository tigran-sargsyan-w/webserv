#ifndef WEBSERV_HPP
#define WEBSERV_HPP

#include "Client.hpp"
#include "Config.hpp"
#include <map>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

class WebServ
{
public:
	WebServ();
	WebServ(const WebServ& other);
	~WebServ();
	WebServ& operator=(const WebServ& other);
	int readFromClient(Client& client);
	int SendToClient(Client& client);

	int setup(std::vector<ServerConfig> servers);
	int run();
	int initListeningSocket();
	int bindSockAddress();
	int acceptConnection();
	void removePollfd(int fd);

private:
	int setNonBlocking(int fd);
	int serverSocket;
	ServerConfig serverConfig;
	std::vector<pollfd> pollFds;
	std::map<int, Client> clients;
};

#endif
