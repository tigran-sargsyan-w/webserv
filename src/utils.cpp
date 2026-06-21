#include "utils.hpp"

#include <cctype>
#include <sstream>

std::string intToString(int num)
{
  std::ostringstream oss;
  oss << num;
  return (oss.str());
}

std::string toLowerCase(const std::string &value)
{
  std::string result;
  size_t index;

  result = value;
  index = 0;
  while (index < result.length())
  {
    result[index] = static_cast<char>(
        std::tolower(static_cast<unsigned char>(result[index])));
    index++;
  }
  return (result);
}
