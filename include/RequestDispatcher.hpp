#ifndef REQUESTDISPATCHER_HPP
#define REQUESTDISPATCHER_HPP

#include "Config.hpp"

class Client;
class CgiManager;
class PollManager;
class Response;

class RequestDispatcher
{
	public:
		enum Result
		{
			RESPONSE_READY,
			ASYNC_STARTED,
			DISPATCH_FAILED
		};

		static Result dispatch(
			Client &client,
			const ServerConfig &server,
			CgiManager &cgiManager,
			PollManager &pollManager);

	private:
		RequestDispatcher();

		static void prepareResponse(
			Client &client,
			const Response &response);
};

#endif