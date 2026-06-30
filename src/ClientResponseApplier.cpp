#include "ClientResponseApplier.hpp"

void ClientResponseApplier::apply(Client &client, const Response &response)
{
	client.responseBuffer = response.toString();
	client.bytesSent = 0;
	client.responseReady = true;
	client.state = WRITING;
}
