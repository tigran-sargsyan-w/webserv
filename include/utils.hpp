#ifndef UTILS_HPP
# define UTILS_HPP

#include <string>

std::string intToString(int num);

enum CheckRequestStatus
{
  PARSE_MORE,
  COMPLETED,
  HEADER_TOO_BIG,
  INVALID
};

enum ResponseStatus
{
  OK = 200,
  ERROR = 404
};

#endif
