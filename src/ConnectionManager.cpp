#include "ConnectionManager.hpp"

ConnectionManager::ConnectionManager() {}

std::map<int, Client> &ConnectionManager::getClients()
{
	return (clients);
}

const std::map<int, Client> &ConnectionManager::getClients() const
{
	return (clients);
}