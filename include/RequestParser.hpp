#ifndef REQUESTPARSER_HPP
# define REQUESTPARSER_HPP

#include <string>
#include "Request.hpp"
#include "RequestInspector.hpp"

class RequestParser
{
  public:
        RequestParser() {};
        int parse(const std::string& rawRequest, Request& req);
        InspectRequestStatus status;

  private: 
};

#endif
