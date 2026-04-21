#ifndef WEBSERV_HPP
# define WEBSERV_HPP

#include <vector>
#include <cerrno>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <poll.h>

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
};

#endif
