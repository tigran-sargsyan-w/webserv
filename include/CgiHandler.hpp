#ifndef CGI_HANDLER_HPP
#define CGI_HANDLER_HPP

#include <map>
#include <string>
#include <vector>

typedef std::map<std::string, std::string> CgiEnv;

struct CgiStandardMetaVariables
{
	CgiEnv values;
};

struct CgiContext
{
	std::string executable;
	std::string scriptPath;
	std::string requestBody;
	CgiStandardMetaVariables standard;
};

class CgiHandler
{
public:
	CgiHandler();
	~CgiHandler();

	static std::string runCgi(const CgiContext &context);

private:
	static void addEnv(CgiEnv &env, const std::string &key, const std::string &value);
	static CgiEnv buildEnvironment(const CgiContext &context);
	static std::vector<std::string> buildEnvironmentStrings(const CgiEnv &env);
	static std::vector<char *> buildEnvironmentPointers(std::vector<std::string> &envStrings);
};

#endif