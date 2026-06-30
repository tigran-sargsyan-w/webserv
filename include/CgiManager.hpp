#ifndef CGIMANAGER_HPP
#define CGIMANAGER_HPP

#include "CgiFdRegistry.hpp"
#include "Client.hpp"
#include "PollManager.hpp"
#include "Config.hpp"
#include <vector>
#include <string>
#include <map>

static const int CGI_TIMEOUT_SECONDS = 10;

class CgiManager
{
public:
	CgiManager();
	CgiManager(const CgiManager &other);
	~CgiManager();
	CgiManager &operator=(const CgiManager &other);

	bool isCgiFd(int fd) const;
    void cleanup(Client &client, PollManager &pollManager);
    int getPollTimeoutMs(const std::map<int, Client> &clients) const;
    int checkTimeouts(std::map<int, Client> &clients, const std::vector<ServerConfig> &configs, PollManager &pollManager);
    int handleEvent(int cgiFd, short revents, std::map<int, Client> &clients, const std::vector<ServerConfig> &configs, PollManager &pollManager);

    int startForClient(Client &client, const RouteConfig &route, const ServerConfig &server, PollManager &pollManager);

private:
	CgiFdRegistry fdRegistry;

    int writeToCgi(Client &client, PollManager &pollManager);
    int readFromCgi(Client &client, PollManager &pollManager);
    int checkFinished(Client &client);
    void finishResponse(Client &client, PollManager &pollManager);
    void failResponse(Client &client, int code, const std::string &message, const std::vector<ServerConfig> &configs, PollManager &pollManager);
};

#endif
