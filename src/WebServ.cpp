#include <iostream>
#include "WebServ.hpp"
#include "RequestParser.hpp"
#include "Request.hpp"

WebServ::WebServ()
{
	std::cout << "WebServ created!\n";
}

WebServ::WebServ(const WebServ& other)
{
	(void) other;
	std::cout << "WebServ copy constructor called!\n";
}

WebServ::~WebServ()
{
	std::cout << "WebServ destroyed!\n";
	close(_serverSocket);
}


WebServ& WebServ::operator=(const WebServ& other)
{
	(void) other;
	std::cout << "WebServ assignement operator called!\n";
	return (*this);
}


int WebServ::setup()
{
	std::cout << "WebServ setup called!\n";

		// 1. Create socket
	
	_serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (_serverSocket == -1)
	{
		std::cerr << "socket() failed: " << std::strerror(errno) << "\n";
		return (1);
	}

	std::cout << "Server socked created, FD = " << _serverSocket << "\n";

  int opt = 1;
  if (setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
  {
    std::cerr << "setsockopt() failed: " << std::strerror(errno) << "\n";
    close(_serverSocket);
    return (1);
  }

	// 2. Setup address for socket

	sockaddr_in addr;
  	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(8080);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(_serverSocket, (sockaddr *) &addr, sizeof(addr)) == -1)
	{
		std::cerr << "Error binding socket\n" << std::strerror(errno) << "\n";
		close(_serverSocket);
		return (1);
	}

	// 3. Socket listening

	if (listen(_serverSocket, 10) == -1)
	{
		std::cerr << "Error on socket listening\n";
		close(_serverSocket);
		return (1);
	}

  std::cout << "Listening on localhost:8080\n";
  return (0);
}

int WebServ::run()
{
	std::cout << "WebServ run called!\n";

	while (true)
	{
		// 4. Accept connections

		int clientSocket = accept(_serverSocket, NULL, NULL);
		if (clientSocket == -1)
		{
			std::cerr << "Error accepting client connection\n";
			close(_serverSocket);
			return (1);
		}


		// 5. Receive data from client

		char buffer[4096];

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
			Request request = RequestParser::parse(std::string(buffer));

			if (request.getMethod().empty())
			{
				std::cout << "Failed to parse request\n";
				close(clientSocket);
				continue;
			}
			if (request.getMethod() != "GET")
			{
				std::cout << "Unsupported HTTP method: " << request.getMethod() << "\n";
				close(clientSocket);
				continue;
			}
			if (request.getPath() == "/")
			{
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
			}
			else
			{
				std::string body = "<html><body><h1>404 Not Found</h1></body></html>";
				std::stringstream ss;
				ss << body.size();


				std::string response =
					"HTTP/1.1 404 Not Found\r\n"
					"Content-Type: text/html\r\n"
					"Content-Length: " + ss.str() + "\r\n"
					"Connection: close\r\n"
					"\r\n"
					+ body;


				send(clientSocket, response.c_str(), response.size(), 0);
			}

			close(clientSocket);
		}
	}
}