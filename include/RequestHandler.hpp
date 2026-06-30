#ifndef REQUESTHANDLER_HPP
#define REQUESTHANDLER_HPP

#include "Config.hpp"
#include "Request.hpp"
#include "Response.hpp"

class RequestHandler
{
	public:
		static Response handleRequest(const Request &request, const RouteConfig &route, const ServerConfig &server);

	private:
		RequestHandler();
		RequestHandler(const RequestHandler &other);
		RequestHandler &operator=(const RequestHandler &other);
		~RequestHandler();
};

#endif
