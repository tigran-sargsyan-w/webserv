#ifndef CGI_EVENT_HANDLER_HPP
# define CGI_EVENT_HANDLER_HPP

# include "CgiFdRegistry.hpp"
# include "Client.hpp"
# include "Config.hpp"
# include "PollManager.hpp"
# include <map>
# include <vector>

class CgiEventHandler
{
	public:
		static int	handleEvent(int cgiFd, short revents,
					std::map<int, Client> &clients,
					const std::vector<ServerConfig> &configs,
					PollManager &pollManager, CgiFdRegistry &fdRegistry);

	private:
		CgiEventHandler();
		static int	writeToCgi(Client &client, PollManager &pollManager,
					CgiFdRegistry &fdRegistry);
		static int	readFromCgi(Client &client, PollManager &pollManager,
					CgiFdRegistry &fdRegistry);
		static int	checkFinished(Client &client);
};

#endif
