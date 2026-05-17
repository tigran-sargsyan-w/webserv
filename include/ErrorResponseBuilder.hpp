#ifndef ERRORRESPONSEBUILDER_HPP
#define ERRORRESPONSEBUILDER_HPP

#include "Response.hpp"

#include <string>

class ErrorResponseBuilder
{
public:
    static Response build(int statusCode, const std::string &message);

private:
    ErrorResponseBuilder();
    ErrorResponseBuilder(const ErrorResponseBuilder &other);
    ErrorResponseBuilder &operator=(const ErrorResponseBuilder &other);
    ~ErrorResponseBuilder();
};

#endif