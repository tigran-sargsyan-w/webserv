#include <iostream>
#include <sstream>
#include <string>
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

	while (true)
	{
		// 4. Accept connections

		int clientSocket = accept(serverSocket, NULL, NULL);
		if (clientSocket == -1)
		{
			std::cerr << "Error accepting client connection\n";
			close(serverSocket);
			return (1);
		}


		// 5. Receive data from client

		char input_buffer[4096] = {0};
		// char output_buffer[25] = "Response from server!\n";
    std::string body = "<html><body><h1>Hello from WebServ</h1></body></html>";
    std::stringstream ss;
    ss << body.size();


    std::string response =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html\r\n"
      "Content-Length " + ss.str() + "\r\n"
      "Connection: close\r\n"
      "\r\n"
      + body;



		recv(clientSocket, input_buffer, sizeof(input_buffer), 0);
		std::cout << "Message from client:\n" << input_buffer << "\n";

		send(clientSocket, response.c_str(), response.size(), 0);
	}


	//close(clientSocket);

	close(serverSocket);
	return (0);
}
