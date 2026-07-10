#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <iostream>
#include <streambuf>

#ifndef WEBSERV_LOG_LEVEL
#define WEBSERV_LOG_LEVEL 1
#endif

class NullBuffer : public std::streambuf
{
protected:
	std::streambuf::int_type overflow(std::streambuf::int_type c);
};

class Logger
{
public:
	static std::ostream	&error(void);
	static std::ostream	&info(void);
	static std::ostream	&debug(void);

	static bool			isErrorEnabled(void);
	static bool			isInfoEnabled(void);
	static bool			isDebugEnabled(void);

private:
	static std::ostream	&nullStream(void);
};

#endif
