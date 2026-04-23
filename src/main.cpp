#include "WebServ.hpp"
#include "ConfigParser.hpp"

#include <exception>
#include <iostream>

int	main(int argc, char **argv)
{
	std::string configPath = "configs/default.conf";
	if (argc > 1)
		configPath = argv[1];

	Config config;
	try
	{
		config = ConfigParser::parseFile(configPath);
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << "\n";
		return (1);
	}
	if (config.servers.empty())
	{
		std::cerr << "No server blocks found in config\n";
		return (1);
	}

	WebServ serv;
	if (serv.setup(config.servers[0]) != 0)
		return (1);
	return (serv.run());
}
