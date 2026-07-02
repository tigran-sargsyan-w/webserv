#ifndef CLIENT_TIMEOUT_HANDLER_HPP
# define CLIENT_TIMEOUT_HANDLER_HPP

# include "Client.hpp"

# include <ctime>
# include <map>
# include <vector>

static const int CLIENT_TIMEOUT_SECONDS = 30;

class ClientTimeoutHandler
{
	public:
		static int	getPollTimeoutMs(const std::map<int, Client> &clients,
					int timeoutSeconds);
		static void	collectExpiredClients(const std::map<int, Client> &clients,
					int timeoutSeconds, std::vector<int> &expiredFds);

	private:
		ClientTimeoutHandler();
		static bool	isExpired(const Client &client, time_t now,
					int timeoutSeconds);
};

#endif