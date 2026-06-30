#ifndef UTILS_HPP
# define UTILS_HPP

#include <string>

std::string intToString(int num);
std::string	toLowerCase(const std::string &value);

enum ResponseStatus
{
  OK = 200,
  ERROR = 404
};

#endif
