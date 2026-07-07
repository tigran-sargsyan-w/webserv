#include "Logger.hpp"

std::streambuf::int_type	NullBuffer::overflow(std::streambuf::int_type c)
{
	return (traits_type::not_eof(c));
}

std::ostream	&Logger::nullStream(void)
{
	static NullBuffer	buffer;
	static std::ostream	stream(&buffer);

	return (stream);
}

bool	Logger::isErrorEnabled(void)
{
	return (WEBSERV_LOG_LEVEL >= 1);
}

bool	Logger::isInfoEnabled(void)
{
	return (WEBSERV_LOG_LEVEL >= 2);
}

bool	Logger::isDebugEnabled(void)
{
	return (WEBSERV_LOG_LEVEL >= 3);
}

std::ostream	&Logger::error(void)
{
	if (isErrorEnabled())
		return (std::cerr);
	return (nullStream());
}

std::ostream	&Logger::info(void)
{
	if (isInfoEnabled())
		return (std::cout);
	return (nullStream());
}

std::ostream	&Logger::debug(void)
{
	if (isDebugEnabled())
		return (std::cout);
	return (nullStream());
}
