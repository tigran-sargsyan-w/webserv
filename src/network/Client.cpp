#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctime>
#include "Client.hpp"

/**
 * @brief Create a client wrapper for a socket descriptor.
 * @param fd - client socket fd
 */
Client::Client(int fd)
    : fd(fd),
      state(READING),
      serverIndex(0),
      bytesSent(0),
      bodyBytesToDiscard(0),
      responseReady(false),
      lastActivity(std::time(NULL)) {}

/**
 * @brief Refresh the client's last activity timestamp.
 */
void Client::touchActivity()
{
  lastActivity = std::time(NULL);
}
