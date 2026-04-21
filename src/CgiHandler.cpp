#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <CgiHandler.hpp>

CgiHandler::CgiHandler() {}
CgiHandler::~CgiHandler() {}

void CgiHandler::runCgi()
{
  int fd[2];
  pid_t pid;
  int status;

  if (pipe(fd) < 0)
    return;
  pid = fork();
  if (pid < 0)
    return;
  if (pid == 0)
  {
    dup2(fd[1], STDOUT_FILENO);
    close(fd[1]);
    close(fd[0]);
    char *argv[] = { (char *)"/usr/bin/python3", (char *)"src/cgi-bin/test.py", NULL};
    execve("/usr/bin/python3", argv, NULL);
    _exit(1);
  }
  dup2(fd[0], STDIN_FILENO);
  close(fd[1]);
  ssize_t bytesRead;
  std::string output;
  char buffer[4096];

  while ((bytesRead = read(fd[0], buffer, sizeof(buffer))))
    output += buffer;
  close(fd[0]);
  waitpid(pid, &status, 0);
  std::cout << output; // Testing CGI purposes
  return;
}
