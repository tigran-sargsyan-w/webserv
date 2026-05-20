#ifndef REQUESTHANDLER_HPP
#define REQUESTHANDLER_HPP

#include "Config.hpp"
#include "Request.hpp"
#include "Response.hpp"

class RequestHandler
{
	public:
		RequestHandler ();
		~RequestHandler ();

		static Response handleRequest (const Request &request, const RouteConfig &route, const ServerConfig &server, const std::string &remoteAddr);
		static Response handleStatic (const Request &request, const RouteConfig &route);
		static Response handlePost (const Request &request, const RouteConfig &route, const ServerConfig &server);
		static Response handleHttpDelete (const Request &request, const RouteConfig &route);
};

#endif
