#include "UriUtils.hpp"

namespace
{
	bool	isHexDigit(char c)
	{
		return (
			(c >= '0' && c <= '9')
			|| (c >= 'a' && c <= 'f')
			|| (c >= 'A' && c <= 'F')
		);
	}

	int	hexToInt(char c)
	{
		if (c >= '0' && c <= '9')
			return (c - '0');
		if (c >= 'a' && c <= 'f')
			return (c - 'a' + 10);
		return (c - 'A' + 10);
	}
}

namespace UriUtils
{
	std::string	getPathWithoutQuery(const std::string &uri)
	{
		size_t	questionMark;

		questionMark = uri.find('?');
		if (questionMark == std::string::npos)
			return (uri);
		return (uri.substr(0, questionMark));
	}

	std::string	getQueryString(const std::string &uri)
	{
		size_t	questionMark;

		questionMark = uri.find('?');
		if (questionMark == std::string::npos)
			return ("");
		return (uri.substr(questionMark + 1));
	}

	std::string	decodePath(const std::string &path)
	{
		std::string	result;
		size_t		i;
		int			value;

		i = 0;
		while (i < path.length())
		{
			if (
				path[i] == '%'
				&& i + 2 < path.length()
				&& isHexDigit(path[i + 1])
				&& isHexDigit(path[i + 2])
			)
			{
				value = hexToInt(path[i + 1]) * 16;
				value += hexToInt(path[i + 2]);
				result += static_cast<char>(value);
				i += 3;
			}
			else
			{
				result += path[i];
				i++;
			}
		}
		return (result);
	}
}