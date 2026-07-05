#include "ConfigParser.hpp"
#include "ConfigValidator.hpp"
#include "ConfigDebug.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

ConfigParser::ConfigParser(const std::vector<ConfigToken> &tokens)
	: tokens(tokens), currentIndex(0) {}

Config ConfigParser::parseFile(const std::string &filePath)
{
	std::ifstream file(filePath.c_str());
	if (!file.is_open())
		throw std::runtime_error("Failed to open config file: " + filePath);

	std::stringstream buffer;
	buffer << file.rdbuf();
	ConfigLexer lexer(buffer.str());
	std::vector<ConfigToken> configTokens = lexer.tokenize();
	// ConfigLexer::debugPrintTokens(configTokens);
	ConfigParser parser(configTokens);
	Config config = parser.parse();
	// ConfigParser::debugPrintConfig(config);
	ConfigValidator::validate(config);
	// ConfigValidator::debugPrintValidation(config);
	return (config);
}

Config ConfigParser::parse()
{
	Config config;

	while (!isAtEnd())
	{
		if (peek().type == TOKEN_EOF)
			break;
		const ConfigToken &token = expectWord("Expected top-level directive");
		if (token.value != "server")
			throw error(token, "Only 'server' blocks are allowed at top level");
		config.servers.push_back(parseServerBlock());
	}
	return (config);
}

ServerConfig ConfigParser::parseServerBlock()
{
	ServerConfig server;
	expect(TOKEN_LBRACE, "Expected '{' after server");

	while (!isAtEnd() && peek().type != TOKEN_RBRACE)
	{
		if (peek().type != TOKEN_WORD)
			throw error(peek(), "Expected server directive");
		if (peek().value == "location")
		{
			expectWord("Expected location");
			server.routes.push_back(parseLocationBlock());
			continue;
		}
		parseServerDirective(server);
	}
	expect(TOKEN_RBRACE, "Expected '}' to close server block");
	return (server);
}

RouteConfig ConfigParser::parseLocationBlock()
{
	RouteConfig route;
	route.path = expectWord("Expected location path").value;
	expect(TOKEN_LBRACE, "Expected '{' after location path");

	while (!isAtEnd() && peek().type != TOKEN_RBRACE)
	{
		if (peek().type != TOKEN_WORD)
			throw error(peek(), "Expected location directive");
		parseLocationDirective(route);
	}
	expect(TOKEN_RBRACE, "Expected '}' to close location block");
	return (route);
}

void ConfigParser::parseServerDirective(ServerConfig &server)
{
	const ConfigToken &directive = expectWord("Expected server directive");

	if (directive.value == "listen")
	{
		std::string value = expectWord("Expected listen value").value;
		size_t colonPos = value.find(':');
		int port = 0;
		if (colonPos == std::string::npos)
		{
			server.listen.host = "0.0.0.0";
			if (!toInt(value, port))
				throw error(previous(), "Invalid listen port");
		}
		else
		{
			server.listen.host = value.substr(0, colonPos);
			if (server.listen.host.empty())
				throw error(previous(), "Invalid listen host");
			if (!toInt(value.substr(colonPos + 1), port))
				throw error(previous(), "Invalid listen port");
		}
		server.listen.port = port;
		expectSemicolon();
		return;
	}
	if (directive.value == "server_name")
	{
		server.serverName = expectWord("Expected server_name value").value;
		expectSemicolon();
		return;
	}
	if (directive.value == "root")
	{
		server.root = expectWord("Expected root value").value;
		expectSemicolon();
		return;
	}
	if (directive.value == "index")
	{
		server.index = expectWord("Expected index value").value;
		expectSemicolon();
		return;
	}
	if (directive.value == "client_max_body_size")
	{
		server.clientMaxBodySize = toSize(expectWord("Expected client_max_body_size value").value);
		expectSemicolon();
		return;
	}
	if (directive.value == "error_page")
	{
		int code = 0;
		if (!toInt(expectWord("Expected error code").value, code))
			throw error(previous(), "Invalid error code");
		std::string path = expectWord("Expected error page path").value;
		server.errorPages[code] = path;
		expectSemicolon();
		return;
	}
	if (directive.value == "client_timeout")
	{
		int timeout = 0;

		if (!toInt(expectWord("Expected client_timeout value").value, timeout))
			throw error(previous(), "Invalid client_timeout value");
		server.clientTimeout = timeout;
		expectSemicolon();
		return;
	}
	throw error(directive, "Unknown server directive: " + directive.value);
}

void ConfigParser::parseLocationDirective(RouteConfig &route)
{
	const ConfigToken &directive = expectWord("Expected location directive");

	if (directive.value == "methods")
	{
		if (peek().type == TOKEN_SEMICOLON)
			throw error(peek(), "Expected at least one HTTP method");
		while (!isAtEnd() && peek().type == TOKEN_WORD)
		{
			HttpMethod method = parseHttpMethod(expectWord("Expected method").value);
			if (method == HTTP_UNKNOWN)
				throw error(previous(), "Unknown HTTP method");
			route.methods.insert(method);
		}
		expectSemicolon();
		return;
	}
	if (directive.value == "root")
	{
		route.root = expectWord("Expected root value").value;
		expectSemicolon();
		return;
	}
	if (directive.value == "index")
	{
		route.index = expectWord("Expected index value").value;
		expectSemicolon();
		return;
	}
	if (directive.value == "autoindex")
	{
		std::string value = expectWord("Expected autoindex value").value;
		if (value != "on" && value != "off")
			throw error(previous(), "autoindex must be 'on' or 'off'");
		route.autoindex = (value == "on");
		expectSemicolon();
		return;
	}
	if (directive.value == "upload_enable")
	{
		std::string value = expectWord("Expected upload_enable value").value;
		if (value != "on" && value != "off")
			throw error(previous(), "upload_enable must be 'on' or 'off'");
		route.uploadEnable = (value == "on");
		expectSemicolon();
		return;
	}
	if (directive.value == "upload_store")
	{
		route.uploadStore = expectWord("Expected upload_store value").value;
		expectSemicolon();
		return;
	}
	if (directive.value == "session_enable")
	{
		std::string value = expectWord("Expected session_enable value").value;
		if (value != "on" && value != "off")
			throw error(previous(), "session_enable must be 'on' or 'off'");
		route.sessionEnable = (value == "on");
		expectSemicolon();
		return;
	}
	if (directive.value == "session_path")
	{
		route.sessionPath = expectWord("Expected session_path value").value;
		expectSemicolon();
		return;
	}
	if (directive.value == "return")
	{
		int code = 0;
		if (!toInt(expectWord("Expected return code").value, code))
			throw error(previous(), "Invalid return code");
		route.returnCode = code;
		route.returnPath = expectWord("Expected return path").value;
		route.hasReturn = true;
		expectSemicolon();
		return;
	}
	if (directive.value == "cgi")
	{
		CgiConfig cgi;
		cgi.extension = expectWord("Expected CGI extension").value;
		cgi.executable = expectWord("Expected CGI executable").value;
		route.cgi.push_back(cgi);
		expectSemicolon();
		return;
	}
	throw error(directive, "Unknown location directive: " + directive.value);
}

const ConfigToken &ConfigParser::peek() const
{
	return (tokens[currentIndex]);
}

const ConfigToken &ConfigParser::previous() const
{
	return (tokens[currentIndex - 1]);
}

bool ConfigParser::isAtEnd() const
{
	return (peek().type == TOKEN_EOF);
}

bool ConfigParser::match(ConfigTokenType type, const std::string &value)
{
	if (isAtEnd() || peek().type != type)
		return (false);
	if (!value.empty() && peek().value != value)
		return (false);
	++currentIndex;
	return (true);
}

const ConfigToken &ConfigParser::expect(ConfigTokenType type, const std::string &message)
{
	if (peek().type != type)
		throw error(peek(), message);
	++currentIndex;
	return (previous());
}

const ConfigToken &ConfigParser::expectWord(const std::string &message)
{
	return (expect(TOKEN_WORD, message));
}

void ConfigParser::expectSemicolon()
{
	expect(TOKEN_SEMICOLON, "Expected ';'");
}

bool ConfigParser::toInt(const std::string &text, int &value) const
{
	char *end = NULL;
	long result = std::strtol(text.c_str(), &end, 10);

	if (*end != '\0')
		return (false);
	value = static_cast<int>(result);
	return (true);
}

size_t ConfigParser::toSize(const std::string &text) const
{
	char *end = NULL;
	unsigned long result = std::strtoul(text.c_str(), &end, 10);

	if (*end != '\0')
		throw error(previous(), "Invalid size value");
	return (static_cast<size_t>(result));
}

std::runtime_error ConfigParser::error(const ConfigToken &token, const std::string &message) const
{
	std::ostringstream oss;
	oss << "Config parse error at line " << token.line << ", column " << token.column << ": " << message;
	return (std::runtime_error(oss.str()));
}
