#include "ConfigParser.hpp"
#include "WebServ.hpp"

#include <exception>
#include <iostream>

int main(int argc, char **argv) {
  std::string configPath = "configs/default.conf";
  if (argc > 1)
    configPath = argv[1];

  Config config;
  try {
    config = ConfigParser::parseFile(configPath);
  } catch (const std::exception &e) {
    std::cerr << e.what() << "\n";
    return (1);
  }
  if (config.servers.empty()) {
    std::cerr << "No server blocks found in config\n";
    return (1);
  }

  WebServ serv;

  if (serv.setup(config.servers) != 0) {
    std::cerr << "All server blocks failed, error setting up WebServ!\n";
    return (1);
  }
  return (serv.run());
}
