#ifndef WEBSERV_HPP
#define WEBSERV_HPP

#include "Client.hpp"
#include "Config.hpp"
#include "PollManager.hpp"
#include "CgiManager.hpp"
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
	WebServ(const WebServ &other);
	~WebServ();
	WebServ &operator=(const WebServ &other);
	int readFromClient(Client &client);
	int SendToClient(Client &client);

	int setup(std::vector<ServerConfig> servers);
	int run();
	int initListeningSocket();
	int bindSockAddress(int listeningSocket, size_t configIndex);
	int acceptConnection(int listeningSocket);
	bool isListeningFd(int fd);

private:
	PollManager pollManager;
	CgiManager cgiManager;

	int setNonBlocking(int fd);
	void closeAndRemoveFd(int fd);
	std::vector<ServerConfig> configs;
	std::map<int, size_t> listenerFdToIndex;
	std::map<int, Client> clients;

	int startCgiForClient(Client &client, const RouteConfig &route);
	int handleCgiEvent(int cgiFd, short revents);
};

#endif
