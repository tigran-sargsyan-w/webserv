#ifndef REQUESTPARSER_HPP
# define REQUESTPARSER_HPP

#include <string>
#include "Request.hpp"

class RequestParser
{
    public:
        RequestParser();
        ~RequestParser();

        static Request parse(const std::string& rawRequest);
};

#endif