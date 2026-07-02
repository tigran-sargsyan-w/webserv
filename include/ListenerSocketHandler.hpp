#ifndef LISTENERSOCKETHANDLER_HPP
#define LISTENERSOCKETHANDLER_HPP

#include "Client.hpp"
#include "Config.hpp"
#include "PollManager.hpp"
#include <cstddef>
#include <map>
#include <vector>

class ListenerSocketHandler
{
	public:
		ListenerSocketHandler();
		ListenerSocketHandler(const ListenerSocketHandler &other);
		~ListenerSocketHandler();
		ListenerSocketHandler &operator=(const ListenerSocketHandler &other);

		int setup(const std::vector<ServerConfig> &configs,
			PollManager &pollManager);
		int acceptConnection(int listeningSocket,
			std::map<int, Client> &clients, PollManager &pollManager);
		bool isListeningFd(int fd) const;
		void removeFd(int fd);

	private:
		std::map<int, size_t> listenerFdToIndex;

		int initListeningSocket(void) const;
		int bindSockAddress(int listeningSocket,
			const ServerConfig &config) const;
		int setupSingleListener(const ServerConfig &config, size_t configIndex,
			PollManager &pollManager);
		int setNonBlocking(int fd) const;
		std::string ipToString(unsigned int address) const;
};

#endif
