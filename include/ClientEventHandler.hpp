#ifndef CLIENTEVENTHANDLER_HPP
#define CLIENTEVENTHANDLER_HPP

#include "CgiManager.hpp"
#include "Client.hpp"
#include "Config.hpp"
#include "PollManager.hpp"

class ClientEventHandler
{
	public:
		enum Result
		{
			EVENT_HANDLED,
			CLIENT_SHOULD_CLOSE,
			EVENT_FAILED
		};

		static Result handle(Client &client, short revents,
			const ServerConfig &server, CgiManager &cgiManager,
			PollManager &pollManager);

	private:
		ClientEventHandler();
};

#endif
