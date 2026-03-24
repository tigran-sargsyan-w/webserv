#ifndef REQUEST_HPP
# define REQUEST_HPP

#include <string>

class Request 
{
    public:
        Request();
        ~Request();

    private:
        std::string _method;
        std::string _path;
        std::string _version;
        std::map<std::string, std::string> _headers;
        std::string _body;
};

#endif