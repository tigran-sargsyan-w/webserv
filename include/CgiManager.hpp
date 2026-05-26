#ifndef CGIMANAGER_HPP
#define CGIMANAGER_HPP

#include "Client.hpp"
#include "PollManager.hpp"
#include <map>

class CgiManager
{
public:
	CgiManager();
	CgiManager(const CgiManager &other);
	~CgiManager();
	CgiManager &operator=(const CgiManager &other);

	bool isCgiFd(int fd) const;
	void registerCgiFd(int cgiFd, int clientFd);
	void unregisterCgiFd(int cgiFd);
	bool getClientFd(int cgiFd, int &clientFd) const;

    void closeCgiFd(int fd, PollManager &pollManager);
    void cleanup(Client &client, PollManager &pollManager);
    void resetState(Client &client);

    int writeToCgi(Client &client, PollManager &pollManager);
    int readFromCgi(Client &client, PollManager &pollManager);
    int checkFinished(Client &client);

private:
	std::map<int, int> cgiFdToClientFd;
};

#endif