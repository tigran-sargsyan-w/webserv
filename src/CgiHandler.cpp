#include <CgiHandler.hpp>
#include <iostream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

CgiHandler::CgiHandler() {}
CgiHandler::~CgiHandler() {}

void CgiHandler::addEnv(CgiEnv &env, const std::string &key, const std::string &value)
{
	env[key] = value;
}

CgiEnv CgiHandler::buildEnvironment(const CgiContext &context)
{
	CgiEnv env;

	env = context.standard.values;
	return (env);
}

std::vector<std::string> CgiHandler::buildEnvironmentStrings(const CgiEnv &env)
{
	std::vector<std::string> result;
	CgiEnv::const_iterator it;

	it = env.begin();
	while (it != env.end())
	{
		result.push_back(it->first + "=" + it->second);
		it++;
	}
	return (result);
}

std::vector<char *> CgiHandler::buildEnvironmentPointers(std::vector<std::string> &envStrings)
{
	std::vector<char *> envp;
	size_t i;

	i = 0;
	while (i < envStrings.size())
	{
		envp.push_back(const_cast<char *>(envStrings[i].c_str()));
		i++;
	}
	envp.push_back(NULL);
	return (envp);
}

static void debugPrintEnv(const CgiEnv &env)
{
	CgiEnv::const_iterator it;

	std::cout << "\n===== CGI ENV DEBUG =====" << std::endl;
	it = env.begin();
	while (it != env.end())
	{
		std::cout << it->first << "=" << it->second << std::endl;
		it++;
	}
	std::cout << "=========================\n"
			  << std::endl;
}

std::string CgiHandler::runCgi(const CgiContext &context)
{
	int pipeFd[2];
	pid_t pid;
	std::string output;
	char buffer[4096];
	CgiEnv env;
	std::vector<std::string> envStrings;
	std::vector<char *> envp;

	if (pipe(pipeFd) == -1)
	{
		std::cerr << "pipe() failed: " << std::strerror(errno) << std::endl;
		return ("");
	}
	env = buildEnvironment(context);
	debugPrintEnv(env);
	envStrings = buildEnvironmentStrings(env);
	envp = buildEnvironmentPointers(envStrings);
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
			const_cast<char *>(context.executable.c_str()),
			const_cast<char *>(context.scriptPath.c_str()),
			NULL};

		execve(context.executable.c_str(), argv, &envp[0]);
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