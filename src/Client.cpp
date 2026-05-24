#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "Client.hpp"

Client::Client(int fd)
    : fd(fd),
      requestValid(false),
      requestReady(false),
      state(READING),
      serverIndex(0),
      bytesSent(0),
      responseReady(false),
      cgiPid(-1),
      cgiStdinFd(-1),
      cgiStdoutFd(-1),
      cgiInputSent(0),
      cgiStdinClosed(true),
      cgiStdoutClosed(true),
      cgiFinished(false),
      cgiStartTime(0) {}