#ifndef CGI_HANDLER_HPP
# define CGI_HANDLER_HPP

#include <string>

class CgiHandler
{
  public:
    CgiHandler();
    ~CgiHandler();

    static std::string runCgi(const std::string &executable, const std::string &scriptPath, const std::string &queryString);
};

#endif

