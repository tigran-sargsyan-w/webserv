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
  bool stop = true;
  for (size_t i = 0; i < config.servers.size(); i++)
  {
    if (serv.setup(config.servers[i]) != 0) {
      std::cerr << "Server Block " << i << " setup failed!\n";
      continue;
    }
    stop = false;
  }
  return (serv.run());
}
