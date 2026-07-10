#ifndef CONFIGPARSER_HPP
#define CONFIGPARSER_HPP

#include <string>
#include <stdexcept>
#include <vector>
#include "Config.hpp"
#include "ConfigLexer.hpp"

class ConfigParser
{
	private:
		explicit ConfigParser(const std::vector<ConfigToken> &tokens);
		Config parse();
		ServerConfig parseServerBlock();
		RouteConfig parseLocationBlock();

		void parseServerDirective(ServerConfig &server);
		void parseLocationDirective(RouteConfig &route);

		const ConfigToken &peek() const;
		const ConfigToken &previous() const;
		bool isAtEnd() const;
		bool match(ConfigTokenType type, const std::string &value);
		const ConfigToken &expect(ConfigTokenType type, const std::string &message);
		const ConfigToken &expectWord(const std::string &message);
		void expectSemicolon();
		bool toInt(const std::string &text, int &value) const;
		size_t toSize(const std::string &text) const;
		std::runtime_error error(const ConfigToken &token, const std::string &message) const;

		std::vector<ConfigToken> tokens;
		size_t currentIndex;

	public:
		static Config parseFile(const std::string &filePath);
		static void debugPrintConfig(const Config &config);
};

#endif