#ifndef CGIMANAGER_HPP
#define CGIMANAGER_HPP

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

private:
	std::map<int, int> cgiFdToClientFd;
};

#endif