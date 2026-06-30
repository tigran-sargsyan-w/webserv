#include "ConfigParser.hpp"
#include "WebServ.hpp"

#include <exception>
#include <iostream>
#include <signal.h>

static int	setupSignals()
{
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
	{
		std::cerr << "Failed to ignore SIGPIPE" << std::endl;
		return (1);
	}
	return (0);
}

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
		std::cerr << e.what() << std::endl;
		return (1);
	}

	if (config.servers.empty())
	{
		std::cerr << "No server blocks found in config" << std::endl;
		return (1);
	}

	if (serv.setup(config.servers) != 0)
	{
		std::cerr << "All server blocks failed, error setting up WebServ!" << std::endl;
		return (1);
	}
	return (serv.run());
}