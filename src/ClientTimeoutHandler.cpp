#include "ClientTimeoutHandler.hpp"

# include <ctime>

bool	ClientTimeoutHandler::isExpired(const Client &client, time_t now,
	int timeoutSeconds)
{
	if (client.cgi.isActive())
		return (false);
	return (now - client.lastActivity >= timeoutSeconds);
}