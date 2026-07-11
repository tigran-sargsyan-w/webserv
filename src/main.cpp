#include "ConfigParser.hpp"
#include "Logger.hpp"
#include "WebServ.hpp"

#include <exception>
#include <signal.h>

/**
 * @brief Sets up signal handlers.
 * @return 0 on success, 1 on error.
 */
static int	setupSignals()
{
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
	{
		Logger::error() << "Failed to ignore SIGPIPE" << std::endl;
		return (1);
	}
	return (0);
}

/**
 * @brief Main function for the web server.
 * @param argc - Number of command-line arguments.
 * @param argv - Command-line arguments.
 * @return 0 on success, 1 on error.
 */
int	main(int argc, char **argv)
{
	std::string	configPath;
	Config		config;
	WebServ		serv;

	if (setupSignals() != 0)
		return (1);

	configPath = "configs/default.conf";
	if (argc > 1)
		configPath = argv[1];

	try
	{
		config = ConfigParser::parseFile(configPath);
	}
	catch (const std::exception &e)
	{
		Logger::error() << e.what() << std::endl;
		return (1);
	}

	if (config.servers.empty())
	{
		Logger::error() << "No server blocks found in config" << std::endl;
		return (1);
	}

	if (serv.setup(config.servers) != 0)
	{
		Logger::error() << "All server blocks failed, error setting up WebServ!" << std::endl;
		return (1);
	}
	return (serv.run());
}
