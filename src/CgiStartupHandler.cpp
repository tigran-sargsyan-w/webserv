#include "CgiStartupHandler.hpp"
#include "CgiCompletionHandler.hpp"
#include "CgiRequestHandler.hpp"
#include "CgiValidator.hpp"

#include <cerrno>
#include <ctime>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

int	CgiStartupHandler::startForClient(Client &client,
	const RouteConfig &route, const ServerConfig &server,
	PollManager &pollManager, CgiFdRegistry &fdRegistry)
{
	CgiContext context;
	CgiProcess process;
	int validationStatus;

	context = CgiRequestHandler::buildContext(client.request, route, server,
		client.getRemoteAddr());

	validationStatus = CgiValidator::validate(context);
	if (validationStatus != 0)
	{
		CgiCompletionHandler::error(client, validationStatus,
			CgiValidator::messageForStatus(validationStatus), server, pollManager);
		return (0);
	}

	if (CgiHandler::startCgi(context, process) != 0)
	{
		CgiCompletionHandler::error(client, 502, "Bad Gateway",
			server, pollManager);
		return (1);
	}

	if (setNonBlockingFd(process.stdinFd) || setNonBlockingFd(process.stdoutFd))
	{
		cleanupFailedProcess(process);
		CgiCompletionHandler::error(client, 500, "Internal Server Error",
			server, pollManager);
		return (1);
	}

	initSession(client, context, process);
	registerProcessFds(client, pollManager, fdRegistry);
	pollManager.setEvents(client.fd, POLLIN);
	return (0);
}

int	CgiStartupHandler::setNonBlockingFd(int fd)
{
	int flags;

	flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
	{
		std::cerr << "fcntl: " << strerror(errno) << "\n";
		return (1);
	}
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
	{
		std::cerr << "fcntl: " << strerror(errno) << "\n";
		return (1);
	}
	return (0);
}

void	CgiStartupHandler::cleanupFailedProcess(CgiProcess &process)
{
	if (process.stdinFd != -1)
		close(process.stdinFd);
	if (process.stdoutFd != -1)
		close(process.stdoutFd);
	if (process.pid > 0)
	{
		kill(process.pid, SIGKILL);
		waitpid(process.pid, NULL, 0);
	}
}

void	CgiStartupHandler::initSession(Client &client,
	const CgiContext &context, const CgiProcess &process)
{
	client.cgi.pid = process.pid;
	client.cgi.stdinFd = process.stdinFd;
	client.cgi.stdoutFd = process.stdoutFd;
	client.cgi.inputBuffer = context.requestBody;
	client.cgi.inputSent = 0;
	client.cgi.outputBuffer.clear();
	client.cgi.stdinClosed = false;
	client.cgi.stdoutClosed = false;
	client.cgi.finished = false;
	client.cgi.startTime = std::time(NULL);
}

void	CgiStartupHandler::registerProcessFds(Client &client,
	PollManager &pollManager, CgiFdRegistry &fdRegistry)
{
	fdRegistry.registerFd(client.cgi.stdoutFd, client.fd);
	pollManager.addFd(client.cgi.stdoutFd, POLLIN);

	if (client.cgi.inputBuffer.empty())
	{
		close(client.cgi.stdinFd);
		client.cgi.stdinFd = -1;
		client.cgi.stdinClosed = true;
		client.state = CGI_READING;
	}
	else
	{
		fdRegistry.registerFd(client.cgi.stdinFd, client.fd);
		pollManager.addFd(client.cgi.stdinFd, POLLOUT);
		client.state = CGI_WRITING;
	}
}
