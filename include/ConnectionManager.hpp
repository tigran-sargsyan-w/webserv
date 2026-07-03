#ifndef CONNECTION_MANAGER_HPP
# define CONNECTION_MANAGER_HPP

# include "Client.hpp"
# include <map>

class ConnectionManager
{
	public:
		ConnectionManager();
		std::map<int, Client> &getClients();
		const std::map<int, Client> &getClients() const;

	private:
		std::map<int, Client> clients;
};

#endif