#ifndef CLIENT_RESPONSE_APPLIER_HPP
#define CLIENT_RESPONSE_APPLIER_HPP

#include "Client.hpp"
#include "Response.hpp"

class ClientResponseApplier
{
	public:
		static void apply(Client &client, const Response &response);

	private:
		ClientResponseApplier();
};

#endif
