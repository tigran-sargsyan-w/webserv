#ifndef REQUESTHANDLER_HPP
# define REQUESTHANDLER_HPP

#include "Request.hpp"
#include "Response.hpp"

class RequestHandler
{
    public:
        RequestHandler();
        ~RequestHandler();

       static Response handleRequest(const Request& request);
};

#endif
