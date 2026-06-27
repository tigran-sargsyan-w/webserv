#ifndef DELETEHANDLER_HPP
#define DELETEHANDLER_HPP

#include "Config.hpp"
#include "Request.hpp"
#include "Response.hpp"

class DeleteHandler
{
	public:
		static Response	handle(const Request &request,
				const RouteConfig &route,
				const ServerConfig &server);

	private:
		DeleteHandler();
		DeleteHandler(const DeleteHandler &other);
		DeleteHandler &operator=(const DeleteHandler &other);
		~DeleteHandler();
};

#endif