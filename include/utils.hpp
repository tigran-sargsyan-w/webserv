#ifndef UTILS_HPP
# define UTILS_HPP

#include <string>

std::string intToString(int num);

enum CheckRequestStatus
{
  IN_PROCESS,
  COMPLETED,
  INVALID
};

enum ResponseStatus
{
  OK = 200,
  ERROR = 404
};

#endif
