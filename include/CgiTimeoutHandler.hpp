#ifndef CGI_TIMEOUT_HANDLER_HPP
# define CGI_TIMEOUT_HANDLER_HPP

# include "CgiFdRegistry.hpp"
# include "Client.hpp"
# include "Config.hpp"
# include "PollManager.hpp"
# include <ctime>
# include <map>
# include <vector>

static const int CGI_TIMEOUT_SECONDS = 10;

class CgiTimeoutHandler
{
	public:
		static int	getPollTimeoutMs(const std::map<int, Client> &clients);
		static int	handleTimeouts(std::map<int, Client> &clients,
					const std::vector<ServerConfig> &configs,
					PollManager &pollManager, CgiFdRegistry &fdRegistry);

	private:
		CgiTimeoutHandler();
		static bool	isExpired(const Client &client, time_t now);
};

#endif
