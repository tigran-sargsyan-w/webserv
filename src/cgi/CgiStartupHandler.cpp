#include "CgiStartupHandler.hpp"
#include "CgiCompletionHandler.hpp"
#include "CgiRequestHandler.hpp"
#include "CgiValidator.hpp"
#include "Logger.hpp"

#include <cerrno>
#include <ctime>
#include <cstring>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

/**
 * @brief Start CGI for a client: validate, spawn, set non-blocking fds, and register.
 * @param client - client requesting CGI
 * @param route - route configuration
 * @param server - server configuration
 * @param pollManager - poll manager to register fds
 * @param fdRegistry - registry to map CGI fds to client fds
 * @return 0 on success, non-zero on failure
 */
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

/**
 * @brief Set an fd to non-blocking mode.
 * @param fd - file descriptor to modify
 * @return 0 on success, 1 on error
 */
int	CgiStartupHandler::setNonBlockingFd(int fd)
{
	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
	{
		Logger::error() << "fcntl: " << strerror(errno) << "\n";
		return (1);
	}
	return (0);
}

/**
 * @brief Clean up resources for a process that failed to initialize.
 * @param process - process info with fds and pid
 */
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

/**
 * @brief Initialize client's CgiSession from context and process info.
 * @param client - client whose session will be initialized
 * @param context - CGI context with request body and paths
 * @param process - spawned process info (pid and fds)
 */
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

/**
 * @brief Register process fds with poll manager and fd registry, set client state.
 * @param client - client owning the CGI session
 * @param pollManager - poll manager to add fds to
 * @param fdRegistry - registry to map fds to client fd
 */
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
