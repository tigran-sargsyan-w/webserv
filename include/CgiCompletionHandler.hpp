#ifndef CGI_COMPLETION_HANDLER_HPP
#define CGI_COMPLETION_HANDLER_HPP

#include "CgiFdRegistry.hpp"
#include "Client.hpp"
#include "Config.hpp"
#include "PollManager.hpp"

#include <string>
#include <vector>

class CgiCompletionHandler
{
	public:
		static void cleanup(Client &client, PollManager &pollManager, CgiFdRegistry &fdRegistry);
		static void finish(Client &client, PollManager &pollManager);
		static void fail(Client &client, int code, const std::string &message, const std::vector<ServerConfig> &configs,
						PollManager &pollManager, CgiFdRegistry &fdRegistry);
		static void error(Client &client, int code, const std::string &message, const ServerConfig &server, PollManager &pollManager);

	private:
		CgiCompletionHandler();
};

#endif
