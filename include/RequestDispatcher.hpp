#ifndef REQUESTDISPATCHER_HPP
#define REQUESTDISPATCHER_HPP

#include "Config.hpp"
#include "Client.hpp"
#include "CgiManager.hpp"
#include "PollManager.hpp"
#include "Response.hpp"

class RequestDispatcher
{
	public:
		enum Result
		{
			RESPONSE_READY,
			ASYNC_STARTED,
			DISPATCH_FAILED
		};

		static Result dispatch(Client &client, const ServerConfig &server,
							CgiManager &cgiManager, PollManager &pollManager);

	private:
		RequestDispatcher();

		static void prepareResponse(Client &client, const Response &response);
};

#endif