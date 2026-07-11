#include "HttpMessageUtils.hpp"

namespace HttpMessageUtils
{
	/**
	 * @brief Find the end of the HTTP header section.
	 */
	bool findHeaderEnd(const std::string &rawMessage, size_t &headerEnd, size_t &bodyStart)
	{
		headerEnd = rawMessage.find("\r\n\r\n");
		if (headerEnd != std::string::npos)
		{
			bodyStart = headerEnd + 4;
			return (true);
		}
		headerEnd = rawMessage.find("\n\n");
		if (headerEnd != std::string::npos)
		{
			bodyStart = headerEnd + 2;
			return (true);
		}
		headerEnd = std::string::npos;
		bodyStart = std::string::npos;
		return (false);
	}

	/**
	 * @brief Remove leading spaces and tabs from a string.
	 */
	void trimLeft(std::string &value)
	{
		while (!value.empty() && (value[0] == ' ' || value[0] == '\t'))
			value.erase(0, 1);
	}

	/**
	 * @brief Remove trailing spaces, tabs and carriage returns from a string.
	 */
	void trimRight(std::string &value)
	{
		while (!value.empty() && (value[value.length() - 1] == ' ' || value[value.length() - 1] == '\t' || value[value.length() - 1] == '\r'))
		{
			value.erase(value.length() - 1);
		}
	}

	/**
	 * @brief Trim both sides of an HTTP header value.
	 */
	void trimHeaderValue(std::string &value)
	{
		trimLeft(value);
		trimRight(value);
	}

	/**
	 * @brief Parse a decimal size string.
	 */
	bool parseSize(const std::string &text, size_t &value)
	{
		const size_t maxBeforeMul = static_cast<size_t>(-1) / 10;
		size_t i;

		if (text.empty())
			return (false);
		value = 0;
		i = 0;
		while (i < text.length())
		{
			if (text[i] < '0' || text[i] > '9')
				return (false);
			if (value > maxBeforeMul)
				return (false);
			value = value * 10 + (text[i] - '0');
			++i;
		}
		return (true);
	}

	/**
	 * @brief Split a raw header line into key and value.
	 */
	bool splitHeaderLine(const std::string &line, std::string &key, std::string &value)
	{
		std::string cleanLine;
		size_t colon;

		cleanLine = line;
		if (!cleanLine.empty() && cleanLine[cleanLine.length() - 1] == '\r')
			cleanLine.erase(cleanLine.length() - 1);
		colon = cleanLine.find(':');
		if (colon == std::string::npos)
			return (false);
		key = cleanLine.substr(0, colon);
		value = cleanLine.substr(colon + 1);
		return (true);
	}
}
