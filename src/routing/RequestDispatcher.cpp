#include "RequestDispatcher.hpp"

#include "CgiRequestHandler.hpp"
#include "HttpMethod.hpp"
#include "Logger.hpp"
#include "RequestHandler.hpp"
#include "Router.hpp"

static bool shouldHandleBeforeCgi(const Client &client, const RouteConfig &route,
								  const ServerConfig &server)
{
	HttpMethod method = parseHttpMethod(client.getRequest().getMethod());

	if (method == HTTP_UNKNOWN)
		return (true);
	if (route.methods.find(method) == route.methods.end())
		return (true);
	if (server.clientMaxBodySize > 0 &&
		client.getRequest().getBody().size() > server.clientMaxBodySize)
		return (true);
	return (false);
}

RequestDispatcher::Result RequestDispatcher::dispatch(Client &client, const ServerConfig &server,
										  CgiManager &cgiManager, PollManager &pollManager)
{
	const RouteConfig &route = Router::resolve(server, client.getRequest().getPath());

	Logger::debug() << "Matched route: " << route.path << std::endl;

	if (CgiRequestHandler::isCgiRequest(client.getRequest(), route))
	{
		if (shouldHandleBeforeCgi(client, route, server))
		{
			Response response = RequestHandler::handleRequest(client.getRequest(), route, server);
			prepareResponse(client, response);
			return (RESPONSE_READY);
		}
		if (cgiManager.startForClient(client, route, server, pollManager) != 0)
			return (DISPATCH_FAILED);
		return (ASYNC_STARTED);
	}

	Response response = RequestHandler::handleRequest(client.getRequest(), route, server);
	prepareResponse(client, response);
	Logger::debug() << "Response ready, size = "
				<< client.responseBuffer.size() << " bytes" << std::endl;

	return (RESPONSE_READY);
}

void RequestDispatcher::prepareResponse(Client &client, const Response &response)
{
	client.responseBuffer = response.toString();
	client.bytesSent = 0;
	client.responseReady = true;
}
