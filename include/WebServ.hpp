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

class WebServ
{
	public:
		WebServ();
		WebServ(const WebServ& other);
		~WebServ();
		WebServ& operator=(const WebServ& other);

		int setup();
		int run();
    int initListeningSocket();
    int bindSockAddress();
    int acceptConnection();
	
	private:
		int _serverSocket;
    std::vector<pollfd> _pollfds;
    std::map<int, Client> _clients;
};

#endif
