#ifndef REQUEST_LINE_HPP
# define REQUEST_LINE_HPP

# include <string>

enum RequestLineStatus
{
	REQUEST_LINE_OK,
	REQUEST_LINE_BAD_REQUEST,
	REQUEST_LINE_URI_TOO_LONG,
	REQUEST_LINE_NOT_IMPLEMENTED
};

class RequestLine
{
	public:
		RequestLine();

		bool				parse(const std::string &line);
		RequestLineStatus	getStatus(void) const;

		const std::string	&getMethod(void) const;
		const std::string	&getUri(void) const;
		const std::string	&getVersion(void) const;

	private:
		std::string			method;
		std::string			uri;
		std::string			version;
		RequestLineStatus	status;

		void				clear(void);
		bool				isSupportedMethod(void) const;
		bool				isValidUri(void) const;
		bool				isSupportedVersion(void) const;
};

#endif