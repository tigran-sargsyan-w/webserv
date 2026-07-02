#ifndef POLLEVENTHANDLER_HPP
#define POLLEVENTHANDLER_HPP

#include "CgiManager.hpp"
#include "Client.hpp"
#include "Config.hpp"
#include "ListenerSocketHandler.hpp"
#include "PollManager.hpp"
#include <map>
#include <poll.h>
#include <vector>

class PollEventHandler
{
	public:
		enum Result
		{
			ADVANCE_INDEX,
			KEEP_INDEX
		};

		static Result handle(const pollfd &pollFd,
			std::map<int, Client> &clients,
			const std::vector<ServerConfig> &configs,
			CgiManager &cgiManager,
			ListenerSocketHandler &listenerSocketHandler,
			PollManager &pollManager);

	private:
		PollEventHandler();
};

#endif
