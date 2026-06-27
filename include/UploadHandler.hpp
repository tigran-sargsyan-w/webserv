#ifndef UPLOADHANDLER_HPP
#define UPLOADHANDLER_HPP

#include "Config.hpp"
#include "Request.hpp"
#include "Response.hpp"

class UploadHandler
{
	public:
		static Response handle(const Request &request, const RouteConfig &route, const ServerConfig &server);

	private:
		UploadHandler();
		UploadHandler(const UploadHandler &other);
		UploadHandler &operator=(const UploadHandler &other);
		~UploadHandler();
};

#endif