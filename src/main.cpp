#include "ConfigParser.hpp"
#include "EventLoop.hpp"
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) {
    std::string configFile = "config/default.conf";
    if (argc >= 2) configFile = argv[1];

    try {
        ConfigParser parser;
        Config config = parser.parse(configFile);

        EventLoop loop(config);
        loop.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
