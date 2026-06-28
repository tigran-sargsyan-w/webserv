#include "CgiValidator.hpp"

#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>

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
