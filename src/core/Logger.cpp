#include "Logger.hpp"

/**
 * @brief Discards stream output.
 * Needed for disabled log levels.
 * @param c - character to ignore
 * @return the non-EOF character state
 */
std::streambuf::int_type	NullBuffer::overflow(std::streambuf::int_type c)
{
	return (traits_type::not_eof(c));
}

/**
 * @brief Returns the null log stream.
 * Used when logging is disabled.
 * @return a stream that discards all output
 */
std::ostream	&Logger::nullStream(void)
{
	static NullBuffer	buffer;
	static std::ostream	stream(&buffer);

	return (stream);
}

/**
 * @brief Checks whether error logging is enabled.
 * @return true when error logs are allowed
 */
bool	Logger::isErrorEnabled(void)
{
	return (WEBSERV_LOG_LEVEL >= 1);
}

/**
 * @brief Checks whether info logging is enabled.
 * @return true when info logs are allowed
 */
bool	Logger::isInfoEnabled(void)
{
	return (WEBSERV_LOG_LEVEL >= 2);
}

/**
 * @brief Checks whether debug logging is enabled.
 * @return true when debug logs are allowed
 */
bool	Logger::isDebugEnabled(void)
{
	return (WEBSERV_LOG_LEVEL >= 3);
}

/**
 * @brief Returns the error output stream.
 * Falls back to a null stream when disabled.
 * @return stderr or a discarded stream
 */
std::ostream	&Logger::error(void)
{
	if (isErrorEnabled())
		return (std::cerr);
	return (nullStream());
}

/**
 * @brief Returns the info output stream.
 * Falls back to a null stream when disabled.
 * @return stdout or a discarded stream
 */
std::ostream	&Logger::info(void)
{
	if (isInfoEnabled())
		return (std::cout);
	return (nullStream());
}

/**
 * @brief Returns the debug output stream.
 * Falls back to a null stream when disabled.
 * @return stdout or a discarded stream
 */
std::ostream	&Logger::debug(void)
{
	if (isDebugEnabled())
		return (std::cout);
	return (nullStream());
}
