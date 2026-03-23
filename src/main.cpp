#include <iostream>
#include <cerrno>
#include <sstream>
#include <string>
#include <cstring>
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
		std::cerr << "socket() failed: " << std::strerror(errno) << "\n";
		return (1);
	}

	std::cout << "Server socked created, FD = " << serverSocket << "\n";

  int opt = 1;
  if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
  {
    std::cerr << "setsockopt() failed: " << std::strerror(errno) << "\n";
    close(serverSocket);
    return (1);
  }

	// 2. Setup address for socket

	sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(8080);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(serverSocket, (sockaddr *) &addr, sizeof(addr)) == -1)
	{
		std::cerr << "Error binding socket\n" << std::strerror(errno) << "\n";
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

  std::cout << "Listening on localhost:8080\n";

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

  char buffer[4096];
  // char output_buffer[25] = "Response from server!\n";

  ssize_t bytes = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
  if (bytes == -1)
  {
    std::cout << "recv() failed\n";
  }
  else if (bytes == 0)
  {
    std::cout << "Client closed connection\n";
  }
  else
  {
    buffer[bytes] = '\0';
    std::cout << "Message from client:\n" << buffer << "\n";
    std::string body = "<html><body><h1>Hello from WebServ</h1></body></html>";
    std::stringstream ss;
    ss << body.size();


    std::string response =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html\r\n"
      "Content-Length: " + ss.str() + "\r\n"
      "Connection: close\r\n"
      "\r\n"
      + body;


    send(clientSocket, response.c_str(), response.size(), 0);
    close(clientSocket);
  }
  }



	close(serverSocket);
	return (0);
}
