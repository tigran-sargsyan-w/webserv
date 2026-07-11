#include "ClientResponseApplier.hpp"

/**
 * @brief Copy a response into the client state.
 * @param client - target client
 * @param response - response to send
 */
void ClientResponseApplier::apply(Client &client, const Response &response)
{
	client.responseBuffer = response.toString();
	client.bytesSent = 0;
	client.responseReady = true;
	client.state = WRITING;
}
