#include <CgiHandler.hpp>
#include <iostream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

static void debugPrintEnv(const std::string &title, const CgiEnv &env);

CgiHandler::CgiHandler() {}
CgiHandler::~CgiHandler() {}

void CgiHandler::addEnv(CgiEnv &env, const std::string &key, const std::string &value)
{
	env[key] = value;
}

void CgiHandler::mergeEnvironment(CgiEnv &dst, const CgiEnv &src)
{
	CgiEnv::const_iterator it;

	it = src.begin();
	while (it != src.end())
	{
		dst[it->first] = it->second;
		it++;
	}
}

CgiEnv CgiHandler::buildEnvironment(const CgiContext &context)
{
	CgiEnv env;
	CgiEnv envStandard;
	CgiEnv envHttp;
	CgiEnv envImplementation;

	envStandard = context.standard.values;
	envHttp = context.httpHeaders.values;
	envImplementation = context.implementation.values;
	debugPrintEnv("CGI ENV - STANDARD VARS", context.standard.values);
	debugPrintEnv("CGI ENV - HTTP HEADERS", context.httpHeaders.values);
	debugPrintEnv("CGI ENV - IMPLEMENTATION VARS", context.implementation.values);
	mergeEnvironment(env, envStandard);
	mergeEnvironment(env, envHttp);
	mergeEnvironment(env, envImplementation);
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

static void debugPrintEnv(const std::string &title, const CgiEnv &env)
{
	CgiEnv::const_iterator it;

	std::cout << "\n===== " << title << " =====" << std::endl;
	it = env.begin();
	while (it != env.end())
	{
		std::cout << it->first << "=" << it->second << std::endl;
		it++;
	}
	std::cout << "===================================\n" << std::endl;
}

int CgiHandler::startCgi(const CgiContext &context, CgiProcess &process)
{
    int stdinPipe[2];
    int stdoutPipe[2];
    pid_t pid;
    CgiEnv env;
    std::vector<std::string> envStrings;
    std::vector<char *> envp;

    if (pipe(stdinPipe) == -1)
    {
        std::cerr << "pipe() failed: " << std::strerror(errno) << std::endl;
        return (1);
    }
    if (pipe(stdoutPipe) == -1)
    {
        std::cerr << "pipe() failed: " << std::strerror(errno) << std::endl;
        close(stdinPipe[0]);
        close(stdinPipe[1]);
        return (1);
    }

    env = buildEnvironment(context);
    envStrings = buildEnvironmentStrings(env);
    envp = buildEnvironmentPointers(envStrings);

    pid = fork();
    if (pid == -1)
    {
        std::cerr << "fork() failed: " << std::strerror(errno) << std::endl;
        close(stdinPipe[0]);
        close(stdinPipe[1]);
        close(stdoutPipe[0]);
        close(stdoutPipe[1]);
        return (1);
    }

    if (pid == 0)
    {
		if (signal(SIGPIPE, SIG_DFL) == SIG_ERR)
			_exit(1);

        close(stdinPipe[1]);
        close(stdoutPipe[0]);

        if (dup2(stdinPipe[0], STDIN_FILENO) == -1)
		{
			close(stdinPipe[0]);
			close(stdoutPipe[1]);
			_exit(1);
		}
		if (dup2(stdoutPipe[1], STDOUT_FILENO) == -1)
		{
			close(stdinPipe[0]);
			close(stdoutPipe[1]);
			_exit(1);
		}

        close(stdinPipe[0]);
        close(stdoutPipe[1]);

		if (chdir(context.workingDirectory.c_str()) == -1)
			_exit(1);

		char *argv[] = {
			const_cast<char *>(context.executable.c_str()),
			const_cast<char *>(context.scriptFileName.c_str()),
			NULL};

		execve(context.executable.c_str(), argv, &envp[0]);
		_exit(1);
	}

    close(stdinPipe[0]);
    close(stdoutPipe[1]);

    process.pid = pid;
    process.stdinFd = stdinPipe[1];
    process.stdoutFd = stdoutPipe[0];

    return (0);
}