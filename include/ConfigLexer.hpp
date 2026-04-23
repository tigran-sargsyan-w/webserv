#ifndef CONFIGLEXER_HPP
#define CONFIGLEXER_HPP

#include <string>
#include <vector>

enum ConfigTokenType
{
	TOKEN_WORD,
	TOKEN_LBRACE,
	TOKEN_RBRACE,
	TOKEN_SEMICOLON,
	TOKEN_EOF
};

struct ConfigToken
{
	ConfigTokenType type;
	std::string value;
	size_t line;
	size_t column;

	ConfigToken() : type(TOKEN_EOF), value(""), line(1), column(1) {}
	ConfigToken(ConfigTokenType tokenType, const std::string &tokenValue, size_t tokenLine, size_t tokenColumn)
		: type(tokenType), value(tokenValue), line(tokenLine), column(tokenColumn) {}
};

class ConfigLexer
{
	private:
		std::string inputText;
		size_t position;
		size_t currentLine;
		size_t currentColumn;

		char peek() const;
		char advance();
		bool isAtEnd() const;
		void skipWhitespaceAndComments();

	public:
		explicit ConfigLexer(const std::string &input);
		std::vector<ConfigToken> tokenize();
		static void debugPrintTokens(const std::vector<ConfigToken> &tokens);
	};

#endif