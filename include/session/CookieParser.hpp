#ifndef COOKIEPARSER_HPP
#define COOKIEPARSER_HPP

#include <map>
#include <string>

class CookieParser
{
	public:
		static std::map<std::string, std::string> parse(const std::string &cookieHeader);
		static std::string getCookieValue(const std::string &cookieHeader, const std::string &name);

	private:
		CookieParser();
		CookieParser(const CookieParser &other);
		CookieParser &operator=(const CookieParser &other);
		~CookieParser();

		static std::string trim(const std::string &value);
};

#endif
