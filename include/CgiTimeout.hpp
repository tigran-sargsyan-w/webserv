#ifndef CGI_TIMEOUT_HPP
# define CGI_TIMEOUT_HPP

# include "Client.hpp"
# include <ctime>
# include <map>

static const int CGI_TIMEOUT_SECONDS = 10;

class CgiTimeout
{
	public:
		static int	getPollTimeoutMs(const std::map<int, Client> &clients,
					int timeoutSeconds);
		static bool	isExpired(const Client &client, time_t now,
					int timeoutSeconds);

	private:
		CgiTimeout();
};

#endif
