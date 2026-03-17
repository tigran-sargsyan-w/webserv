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

	// 1. Create socket
	
	int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSocket == -1)
	{
		std::cerr << "socket() failed\n";
		return (1);
	}

	std::cout << "Server socked created, FD = " << serverSocket << "\n";

	// 2. Setup address for socket

	sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(8080);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(serverSocket, (sockaddr *) &addr, sizeof(addr)) == -1)
	{
		std::cerr << "Error binding socket\n";
		close(serverSocket);
		return (1);
	}

	// 3. Socket listening

	if (listen(serverSocket, 10) == -1)
	{
		std::cerr << "Error on socket listening\n";
		close(serverSocket);
		return (1);
	}

	// 4. Accept connections

	int clientSocket = accept(serverSocket, NULL, NULL);
	if (clientSocket == -1)
	{
		std::cerr << "Error accepting client connection\n";
		close(serverSocket);
		return (1);
	}


	// 5. Receive data from client

	char buffer[1024] = {0};
	recv(clientSocket, buffer, sizeof(buffer), 0);
	std::cout << "Message from client:\n" << buffer << "\n";

	close(serverSocket);
	return (0);
}
