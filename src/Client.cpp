#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctime>
#include "Client.hpp"

Client::Client(int fd)
    : fd(fd),
      requestValid(false),
      requestReady(false),
      state(READING),
      serverIndex(0),
      bytesSent(0),
      bodyBytesToDiscard(0),
      responseReady(false),
      lastActivity(std::time(NULL)) {}
