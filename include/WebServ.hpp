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
	int bindSockAddress(int listeningSocket, size_t configIndex);
	int acceptConnection(int listeningSocket);
	void removePollfd(int fd);
  bool isListeningFd(int fd);

private:
	int setNonBlocking(int fd);
  std::vector<ServerConfig> configs;
  std::map<int, size_t> listenerFdToIndex;
	std::map<int, Client> clients;
	std::vector<pollfd> pollFds;
};

#endif
