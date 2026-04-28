#ifndef REQUESTPARSER_HPP
# define REQUESTPARSER_HPP

#include <string>
#include "Request.hpp"
#include "utils.hpp"

class RequestParser
{
  public:
        RequestParser() {};
        CheckRequestStatus checkRequest(const std::string& rawRequest);
        int parse(const std::string& rawRequest, Request& req);
};

#endif
