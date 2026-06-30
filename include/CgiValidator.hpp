#ifndef CGI_VALIDATOR_HPP
# define CGI_VALIDATOR_HPP

# include "CgiHandler.hpp"
# include <string>

class CgiValidator
{
	public:
		static int			validate(const CgiContext &context);
		static std::string	messageForStatus(int statusCode);

	private:
		static int	checkScriptPath(const std::string &scriptPath);
		static int	checkExecutablePath(const std::string &executablePath);
};

#endif
