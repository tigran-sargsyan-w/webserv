#include "RequestDispatcher.hpp"

#include "CgiRequestHandler.hpp"
#include "RequestHandler.hpp"
#include "Router.hpp"

#include <iostream>

RequestDispatcher::Result RequestDispatcher::dispatch(Client &client, const ServerConfig &server,
													  CgiManager &cgiManager, PollManager &pollManager)
{
	const RouteConfig &route = Router::resolve(server, client.getRequest().getPath());

	std::cout << "Matched route: " << route.path << std::endl;

	if (CgiRequestHandler::isCgiRequest(client.getRequest(), route))
	{
		if (cgiManager.startForClient(client, route, server, pollManager) != 0)
			return (DISPATCH_FAILED);
		return (ASYNC_STARTED);
	}

	Response response = RequestHandler::handleRequest(client.getRequest(), route, server);
	prepareResponse(client, response);
	std::cout << "Response to client:\n\n"
			  << client.responseBuffer << std::endl;

	return (RESPONSE_READY);
}

void RequestDispatcher::prepareResponse(Client &client, const Response &response)
{
	client.responseBuffer = response.toString();
	client.bytesSent = 0;
	client.responseReady = true;
}