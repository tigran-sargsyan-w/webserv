#ifndef CONNECTION_MANAGER_HPP
# define CONNECTION_MANAGER_HPP

# include "Client.hpp"
# include "Config.hpp"
# include "CgiManager.hpp"
# include "ListenerSocketHandler.hpp"
# include "PollManager.hpp"
# include <vector>
# include <map>

class ConnectionManager
{
	public:
		ConnectionManager();
		std::map<int, Client> &getClients();
		const std::map<int, Client> &getClients() const;
        int getPollTimeoutMs(const std::vector<ServerConfig> &configs) const;
        void enforceTimeouts(const std::vector<ServerConfig> &configs, CgiManager &cgiManager,
            ListenerSocketHandler &listenerSocketHandler, PollManager &pollManager);

	private:
		std::map<int, Client> clients;
};

#endif