#include "ConfigLexer.hpp"
#include "ConfigDebug.hpp"
#include "Logger.hpp"

/**
 * @brief Create a lexer for the provided config text.
 * @param input - raw config input text
 */
ConfigLexer::ConfigLexer(const std::string &input)
	: inputText(input), position(0), currentLine(1), currentColumn(1) {}

/**
 * @brief Return the current character without consuming it.
 * @return current character or '\0' at end of input
 */
char ConfigLexer::peek() const
{
	if (isAtEnd())
		return ('\0');
	return (inputText[position]);
}

/**
 * @brief Consume and return the current character.
 * @return consumed character or '\0' at end of input
 */
char ConfigLexer::advance()
{
	if (isAtEnd())
		return ('\0');
	char currentChar = inputText[position++];
	if (currentChar == '\n')
	{
		++currentLine;
		currentColumn = 1;
	}
	else
	{
		++currentColumn;
	}
	return (currentChar);
}

/**
 * @brief Check whether the lexer reached the end of input.
 * @return true when no more characters are available
 */
bool ConfigLexer::isAtEnd() const
{
	return (position >= inputText.size());
}

/**
 * @brief Skip spaces, newlines, and line comments.
 */
void ConfigLexer::skipWhitespaceAndComments()
{
	while (!isAtEnd())
	{
		char currentChar = peek();
		if (currentChar == ' ' || currentChar == '\t' || currentChar == '\r' || currentChar == '\n')
		{
			advance();
			continue;
		}
		if (currentChar == '#')
		{
			while (!isAtEnd() && peek() != '\n')
				advance();
			continue;
		}
		break;
	}
}

/**
 * @brief Convert the config text into a token stream.
 * @return token sequence ending with EOF
 */
std::vector<ConfigToken> ConfigLexer::tokenize()
{
	std::vector<ConfigToken> tokens;

	while (!isAtEnd())
	{
		skipWhitespaceAndComments();
		if (isAtEnd())
			break;
		size_t startLine = currentLine;
		size_t startColumn = currentColumn;
		char currentChar = peek();
		if (currentChar == '{')
		{
			advance();
			tokens.push_back(ConfigToken(TOKEN_LBRACE, "{", startLine, startColumn));
			continue;
		}
		if (currentChar == '}')
		{
			advance();
			tokens.push_back(ConfigToken(TOKEN_RBRACE, "}", startLine, startColumn));
			continue;
		}
		if (currentChar == ';')
		{
			advance();
			tokens.push_back(ConfigToken(TOKEN_SEMICOLON, ";", startLine, startColumn));
			continue;
		}

		std::string word;
		while (!isAtEnd())
		{
			currentChar = peek();
			if (currentChar == ' ' || currentChar == '\t' || currentChar == '\r' || currentChar == '\n' || currentChar == '{' || currentChar == '}' || currentChar == ';')
				break;
			word.push_back(advance());
		}
		if (!word.empty())
			tokens.push_back(ConfigToken(TOKEN_WORD, word, startLine, startColumn));
	}
	tokens.push_back(ConfigToken(TOKEN_EOF, "", currentLine, currentColumn));
	return (tokens);
}

/**
 * @brief Convert a token type to a readable label.
 * @param tokenType - token kind to stringify
 * @return short token type name
 */
static const char *tokenTypeToString(ConfigTokenType tokenType)
{
	if (tokenType == TOKEN_WORD)
		return ("WORD");
	if (tokenType == TOKEN_LBRACE)
		return ("LBRACE");
	if (tokenType == TOKEN_RBRACE)
		return ("RBRACE");
	if (tokenType == TOKEN_SEMICOLON)
		return ("SEMICOLON");
	return ("EOF");
}

/**
 * @brief Print tokens when debug logging is enabled.
 * @param tokens - token list to display
 */
void ConfigLexer::debugPrintTokens(const std::vector<ConfigToken> &tokens)
{
	if (!Logger::isDebugEnabled())
		return;
	Logger::debug() << ConfigDebug::lexer << "[lexer] tokens" << ConfigDebug::reset << "\n";
	for (size_t index = 0; index < tokens.size(); ++index)
	{
		const ConfigToken &token = tokens[index];
		Logger::debug() << ConfigDebug::lexer
			<< "  - " << tokenTypeToString(token.type)
			<< " @ " << token.line << ":" << token.column;
		if (!token.value.empty())
			Logger::debug() << " => '" << token.value << "'";
		Logger::debug() << ConfigDebug::reset << "\n";
	}
}
