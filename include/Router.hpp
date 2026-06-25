#ifndef ROUTER_HPP
#define ROUTER_HPP

#include "Config.hpp"

#include <string>

class Router
{
	public:
		static const RouteConfig	&resolve(
			const ServerConfig &server,
			const std::string &requestTarget);

	private:
		Router();
		static bool	matches(
			const std::string &routePath,
			const std::string &requestPath);
};

#endif