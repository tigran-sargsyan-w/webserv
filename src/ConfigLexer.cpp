#include "ConfigLexer.hpp"
#include "ConfigDebug.hpp"

#include <iostream>

ConfigLexer::ConfigLexer(const std::string &input)
	: inputText(input), position(0), currentLine(1), currentColumn(1)
{
}

char ConfigLexer::peek() const
{
	if (isAtEnd())
		return ('\0');
	return (inputText[position]);
}

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

bool ConfigLexer::isAtEnd() const
{
	return (position >= inputText.size());
}

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

void ConfigLexer::debugPrintTokens(const std::vector<ConfigToken> &tokens)
{
	std::cout << ConfigDebug::lexer << "[lexer] tokens" << ConfigDebug::reset << "\n";
	for (size_t index = 0; index < tokens.size(); ++index)
	{
		const ConfigToken &token = tokens[index];
		std::cout << ConfigDebug::lexer
			<< "  - " << tokenTypeToString(token.type)
			<< " @ " << token.line << ":" << token.column;
		if (!token.value.empty())
			std::cout << " => '" << token.value << "'";
		std::cout << ConfigDebug::reset << "\n";
	}
}
