#ifndef CGI_STARTUP_HANDLER_HPP
# define CGI_STARTUP_HANDLER_HPP

# include "CgiFdRegistry.hpp"
# include "CgiHandler.hpp"
# include "Client.hpp"
# include "Config.hpp"
# include "PollManager.hpp"

class CgiStartupHandler
{
	public:
		static int	startForClient(Client &client, const RouteConfig &route,
					const ServerConfig &server, PollManager &pollManager,
					CgiFdRegistry &fdRegistry);

	private:
		CgiStartupHandler();
		static int	setNonBlockingFd(int fd);
		static void	cleanupFailedProcess(CgiProcess &process);
		static void	initSession(Client &client, const CgiContext &context,
					const CgiProcess &process);
		static void	registerProcessFds(Client &client, PollManager &pollManager,
					CgiFdRegistry &fdRegistry);
};

#endif
