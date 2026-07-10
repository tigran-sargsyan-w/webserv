#ifndef CGI_FD_REGISTRY_HPP
# define CGI_FD_REGISTRY_HPP

# include "PollManager.hpp"
# include <map>

class CgiFdRegistry
{
	public:
		CgiFdRegistry();
		CgiFdRegistry(const CgiFdRegistry &other);
		~CgiFdRegistry();
		CgiFdRegistry &operator=(const CgiFdRegistry &other);

		bool	contains(int cgiFd) const;
		void	registerFd(int cgiFd, int clientFd);
		void	unregisterFd(int cgiFd);
		bool	getClientFd(int cgiFd, int &clientFd) const;
		void	closeFd(int cgiFd, PollManager &pollManager);

	private:
		std::map<int, int> fdToClientFd;
};

#endif
