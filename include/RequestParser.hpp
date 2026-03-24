#ifndef REQUESTPARSER_HPP
# define REQUESTPARSER_HPP

class RequestParser
{
    public:
        static Request parse(const std::string& rawRequest);
};

#endif