#include "Utils.hpp"

#include <cctype>
#include <sstream>

/**
 * @brief Converts an integer to a string.
 * @param num - Integer value to convert.
 * @return String representation of the integer.
 */
std::string intToString(int num)
{
  std::ostringstream oss;
  oss << num;
  return (oss.str());
}

/**
 * @brief Converts a string to lowercase.
 * @param value - Input string.
 * @return Lowercase copy of the input string.
 */
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
