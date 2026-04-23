#ifndef WEBSERV_HPP
# define WEBSERV_HPP

#include <iostream>
#include <cerrno>
#include <sstream>
#include <string>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include "Config.hpp"

class WebServ
{
	private:
		int _serverSocket;

	public:
		WebServ();
		WebServ(const WebServ& other);
		~WebServ();
		WebServ& operator=(const WebServ& other);

		int setup(const ServerConfig &serverConfig);
		int run();
};

#endif
