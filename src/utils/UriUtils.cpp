#include "UriUtils.hpp"

namespace
{
	/**
	 * @brief Checks whether a character is a hexadecimal digit.
	 * @param c - Character to validate.
	 * @return True if the character is hex, otherwise false.
	 */
	bool isHexDigit(char c)
	{
		return (
			(c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
	}

	/**
	 * @brief Converts a hexadecimal digit to its integer value.
	 * @param c - Hex character.
	 * @return Numeric value of the hex digit.
	 */
	int hexToInt(char c)
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
	/**
	 * @brief Removes the query string from a URI.
	 * @param uri - Full URI string.
	 * @return URI path without the query part.
	 */
	std::string getPathWithoutQuery(const std::string &uri)
	{
		size_t questionMark;

		questionMark = uri.find('?');
		if (questionMark == std::string::npos)
			return (uri);
		return (uri.substr(0, questionMark));
	}

	/**
	 * @brief Extracts the query string from a URI.
	 * @param uri - Full URI string.
	 * @return Query string, or an empty string if absent.
	 */
	std::string getQueryString(const std::string &uri)
	{
		size_t questionMark;

		questionMark = uri.find('?');
		if (questionMark == std::string::npos)
			return ("");
		return (uri.substr(questionMark + 1));
	}

	/**
	 * @brief Decodes percent-encoded path segments.
	 * @param path - Encoded path string.
	 * @return Decoded path string.
	 */
	std::string decodePath(const std::string &path)
	{
		std::string result;
		size_t i;
		int value;

		i = 0;
		while (i < path.length())
		{
			if (
				path[i] == '%' && i + 2 < path.length() && isHexDigit(path[i + 1]) && isHexDigit(path[i + 2]))
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