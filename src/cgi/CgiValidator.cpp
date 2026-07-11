#include "CgiValidator.hpp"

#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>

/**
 * @brief Convert a CGI-related status code into a human-readable message.
 * @param statusCode - HTTP-like status code
 * @return short description string
 */
std::string	CgiValidator::messageForStatus(int statusCode)
{
	if (statusCode == 403)
		return ("Forbidden");
	if (statusCode == 404)
		return ("Not Found");
	if (statusCode == 502)
		return ("Bad Gateway");
	return ("Internal Server Error");
}

/**
 * @brief Validate the CGI script path exists and is readable.
 * @param scriptPath - path to the script file
 * @return 0 if ok, 404 if not found, 403 if inaccessible
 */
int	CgiValidator::checkScriptPath(const std::string &scriptPath)
{
	struct stat	scriptStat;

	if (scriptPath.empty())
		return (404);
	if (stat(scriptPath.c_str(), &scriptStat) == -1)
	{
		if (errno == ENOENT || errno == ENOTDIR)
			return (404);
		return (403);
	}
	if (S_ISDIR(scriptStat.st_mode))
		return (403);
	if (access(scriptPath.c_str(), R_OK) == -1)
		return (403);
	return (0);
}

/**
 * @brief Validate the CGI executable exists and is executable.
 * @param executablePath - path to the CGI executable
 * @return 0 if ok, 502 on problems
 */
int	CgiValidator::checkExecutablePath(const std::string &executablePath)
{
	struct stat	executableStat;

	if (executablePath.empty())
		return (502);
	if (stat(executablePath.c_str(), &executableStat) == -1)
		return (502);
	if (S_ISDIR(executableStat.st_mode))
		return (502);
	if (access(executablePath.c_str(), X_OK) == -1)
		return (502);
	return (0);
}

/**
 * @brief Validate both script and executable paths in the context.
 * @param context - CGI context to validate
 * @return 0 if valid, otherwise an HTTP-like error code
 */
int	CgiValidator::validate(const CgiContext &context)
{
	int	statusCode;

	statusCode = checkScriptPath(context.scriptPath);
	if (statusCode != 0)
		return (statusCode);
	statusCode = checkExecutablePath(context.executable);
	if (statusCode != 0)
		return (statusCode);
	return (0);
}
