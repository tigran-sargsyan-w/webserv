#include <sstream>

std::string intToString(int num)
{
  std::ostringstream oss;
  oss << num;
  return (oss.str());
}

