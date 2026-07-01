#include "CgiCompletionHandler.hpp"
#include "CgiRequestHandler.hpp"
#include "CgiValidator.hpp"
#include "ClientResponseApplier.hpp"
#include "ErrorResponseHandler.hpp"
#include "Response.hpp"

#include <signal.h>
#include <sys/wait.h>

void	CgiCompletionHandler::cleanup(Client &client, PollManager &pollManager,
	CgiFdRegistry &fdRegistry)
{
	if (client.cgi.stdinFd != -1)
		fdRegistry.closeFd(client.cgi.stdinFd, pollManager);
	if (client.cgi.stdoutFd != -1)
		fdRegistry.closeFd(client.cgi.stdoutFd, pollManager);
	if (client.cgi.pid > 0)
	{
		kill(client.cgi.pid, SIGKILL);
		waitpid(client.cgi.pid, NULL, 0);
	}
	client.cgi.reset();
}

void	CgiCompletionHandler::finish(Client &client, PollManager &pollManager)
{
	Response response;

	response = CgiRequestHandler::buildResponse(client.cgi.outputBuffer);
	ClientResponseApplier::apply(client, response);
	client.cgi.reset();
	pollManager.setEvents(client.fd, POLLOUT);
}

void	CgiCompletionHandler::fail(Client &client, int code,
	const std::string &message, const std::vector<ServerConfig> &configs,
	PollManager &pollManager, CgiFdRegistry &fdRegistry)
{
	const ServerConfig &server = configs[client.serverIndex];

	cleanup(client, pollManager, fdRegistry);
	error(client, code, message, server, pollManager);
}

void	CgiCompletionHandler::error(Client &client, int code,
	const std::string &message, const ServerConfig &server,
	PollManager &pollManager)
{
	Response response;

	response = ErrorResponseHandler::build(code, message, server);
	ClientResponseApplier::apply(client, response);
	pollManager.setEvents(client.fd, POLLOUT);
}

void	CgiCompletionHandler::validationError(Client &client,
	const ServerConfig &server, int statusCode, PollManager &pollManager)
{
	error(client, statusCode, CgiValidator::messageForStatus(statusCode),
		server, pollManager);
}
