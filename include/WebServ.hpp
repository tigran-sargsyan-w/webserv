#ifndef WEBSERV_HPP
# define WEBSERV_HPP

#include <map>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <poll.h>
#include "Client.hpp"
#include "Config.hpp"

class WebServ
{

	public:
		WebServ();
		WebServ(const WebServ& other);
		~WebServ();
		WebServ& operator=(const WebServ& other);
    int  readFromClient(Client& client);
    int  SendToClient(Client& client);

		int setup(const ServerConfig &serverConfig);
		int run();
    int initListeningSocket();
    int bindSockAddress();
    int acceptConnection();
    void  removePollfd(int fd);
	
	private:
		int serverSocket;
    std::vector<pollfd> _pollfds;
    std::map<int, Client> _clients;
};

#endif
