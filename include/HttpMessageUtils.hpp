#ifndef HTTP_MESSAGE_UTILS_HPP
# define HTTP_MESSAGE_UTILS_HPP

# include <cstddef>
# include <string>

namespace HttpMessageUtils
{
	bool	findHeaderEnd(const std::string &rawMessage,
					size_t &headerEnd,
					size_t &bodyStart);
	void	trimLeft(std::string &value);
	void	trimRight(std::string &value);
	void	trimHeaderValue(std::string &value);
	bool	parseSize(const std::string &text, size_t &value);
	bool	splitHeaderLine(const std::string &line,
					std::string &key,
					std::string &value);
}

#endif
