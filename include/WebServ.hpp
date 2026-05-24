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
	void removePollfd(int fd);
	bool isListeningFd(int fd);

private:
	int setNonBlocking(int fd);
	void closeAndRemoveFd(int fd);
	std::vector<ServerConfig> configs;
	std::map<int, size_t> listenerFdToIndex;
	std::map<int, Client> clients;
	std::vector<pollfd> pollFds;
	std::map<int, int> cgiFdToClientFd;

	bool isCgiFd(int fd) const;
	void registerPollFd(int fd, short events);
	void setPollEvents(int fd, short events);

	int startCgiForClient(Client &client, const RouteConfig &route);
	int handleCgiEvent(int cgiFd, short revents);
	int writeToCgi(Client &client);
	int readFromCgi(Client &client);
	void closeCgiFd(int fd);
	void cleanupCgi(Client &client);
	int checkCgiFinished(Client &client);
	int getPollTimeoutMs(void) const;
	int checkCgiTimeouts(void);
	void finishCgiResponse(Client &client);
	void failCgiResponse(Client &client, int code, const std::string &message);
	void resetCgiState(Client &client);
};

#endif
