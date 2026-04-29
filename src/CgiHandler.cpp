#include <CgiHandler.hpp>
#include <iostream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

CgiHandler::CgiHandler() {}
CgiHandler::~CgiHandler() {}

std::string CgiHandler::runCgi(const std::string &executable, const std::string &scriptPath, const std::string &queryString)
{
	int pipeFd[2];
	pid_t pid;
	std::string output;
	char buffer[4096];

	if (pipe(pipeFd) == -1)
	{
		std::cerr << "pipe() failed: " << std::strerror(errno) << std::endl;
		return ("");
	}

	pid = fork();
	if (pid == -1)
	{
		std::cerr << "fork() failed: " << std::strerror(errno) << std::endl;
		close(pipeFd[0]);
		close(pipeFd[1]);
		return ("");
	}

	if (pid == 0)
	{
		close(pipeFd[0]);

		if (dup2(pipeFd[1], STDOUT_FILENO) == -1)
		{
			close(pipeFd[1]);
			_exit(1);
		}

		close(pipeFd[1]);

		char *argv[] = {
			const_cast<char *>(executable.c_str()),
			const_cast<char *>(scriptPath.c_str()),
			NULL};

		std::string queryEnv = "QUERY_STRING=" + queryString;
		char *envp[] = {
			const_cast<char *>(queryEnv.c_str()),
			NULL};

		execve(executable.c_str(), argv, envp);

		_exit(1);
	}

	close(pipeFd[1]);

	ssize_t bytesRead = 0;
	while ((bytesRead = read(pipeFd[0], buffer, sizeof(buffer))) > 0)
	{
		output.append(buffer, bytesRead);
	}

	close(pipeFd[0]);

	int status = 0;
	waitpid(pid, &status, 0);

	if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
	{
		std::cerr << "CGI exited with status "
				  << WEXITSTATUS(status) << std::endl;
	}

	return (output);
}
