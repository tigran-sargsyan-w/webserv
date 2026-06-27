#ifndef STORAGEPATHRESOLVER_HPP
#define STORAGEPATHRESOLVER_HPP

#include "Config.hpp"
#include "Request.hpp"

#include <string>

namespace StoragePathResolver
{
	std::string resolve(const Request &request, const RouteConfig &route);
}

#endif