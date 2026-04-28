#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "Client.hpp"

Client::Client(int fd) : fd(fd), requestValid(false), requestReady(false), state(READING)  {}

