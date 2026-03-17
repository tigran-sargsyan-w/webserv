#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include "WebServ.hpp"

int	main()
{
	std::cout << "Hello, this is WebServ!\n";

	WebServ serv;

	int serverFd = socket(AF_INET, SOCK_STREAM, 0);
	if (serverFd == -1)
	{
		std::cerr << "socket() failed\n";
		return (1);
	}

	std::cout << "Server socked created, FD = " << serverFd << "\n";


	sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(8080);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(serverFd, (sockaddr *) &addr, sizeof(addr)))
	{
		std::cerr << "Error binding socket\n";
		return (1);
	}

	close(serverFd);
	return (0);
}
