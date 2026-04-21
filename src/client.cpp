#include <cstring>
#include <errno.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

int main() {
  int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

  sockaddr_in serverAddress;

  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(8080);
  serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

  if (connect(clientSocket, (sockaddr *)&serverAddress,
              sizeof(serverAddress)) == -1) {
    std::cerr << "connect() failed: " << strerror(errno) << "\n";
    close(clientSocket);
    return (1);
  }

  const char *request = "GET /HTTP/1.1\r\n"
                        "Content-Type: text/html\r\n"
                        "Host: localhost:8080\r\n"
                        "\r\n";

  send(clientSocket, request, strlen(request), 0);

  char buffer[4096];
  ssize_t bytesReceived;

  while ((bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0)) >
         0) {
    buffer[bytesReceived] = '\0';
    std::cout << buffer;
  }

  if (bytesReceived == -1) {
    std::cerr << "recv() failed: " << strerror(errno) << "\n";
  }
  close(clientSocket);
  return (0);
}
