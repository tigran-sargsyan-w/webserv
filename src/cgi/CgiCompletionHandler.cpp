#include "CgiCompletionHandler.hpp"
#include "CgiRequestHandler.hpp"
#include "ClientResponseApplier.hpp"
#include "ErrorResponseHandler.hpp"
#include "Response.hpp"

#include <signal.h>
#include <sys/wait.h>

/**
 * @brief Clean up CGI-related resources for a client.
 * @param client - client whose CGI session to clean
 * @param pollManager - poll manager to update
 * @param fdRegistry - registry of CGI fds
 */
void CgiCompletionHandler::cleanup(Client &client, PollManager &pollManager, CgiFdRegistry &fdRegistry)
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

/**
 * @brief Finalize a successful CGI execution and send response.
 * @param client - client to apply the response to
 * @param pollManager - poll manager to set write events
 */
void CgiCompletionHandler::finish(Client &client, PollManager &pollManager)
{
	Response response;

	response = CgiRequestHandler::buildResponse(client.cgi.outputBuffer);
	ClientResponseApplier::apply(client, response);
	client.cgi.reset();
	pollManager.setEvents(client.fd, POLLOUT);
}

/**
 * @brief Handle a failed CGI execution: cleanup and send error.
 * @param client - client with the failing CGI session
 * @param code - HTTP status code to send
 * @param message - error message text
 * @param configs - server configurations vector
 * @param pollManager - poll manager for events
 * @param fdRegistry - registry of CGI fds
 */
void CgiCompletionHandler::fail(Client &client, int code, const std::string &message, const std::vector<ServerConfig> &configs,
								PollManager &pollManager, CgiFdRegistry &fdRegistry)
{
	const ServerConfig &server = configs[client.serverIndex];

	cleanup(client, pollManager, fdRegistry);
	error(client, code, message, server, pollManager);
}

/**
 * @brief Send an error response for a CGI failure without extra cleanup.
 * @param client - client to send the error to
 * @param code - HTTP status code
 * @param message - error message text
 * @param server - server configuration used to build the response
 * @param pollManager - poll manager to set write events
 */
void CgiCompletionHandler::error(Client &client, int code, const std::string &message, const ServerConfig &server,
								 PollManager &pollManager)
{
	Response response;

	response = ErrorResponseHandler::build(code, message, server);
	ClientResponseApplier::apply(client, response);
	pollManager.setEvents(client.fd, POLLOUT);
}
