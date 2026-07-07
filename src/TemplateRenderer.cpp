#include "TemplateRenderer.hpp"

#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace
{
	bool	isUrlSafeChar(unsigned char c)
	{
		if (std::isalnum(c))
			return (true);
		if (c == '-' || c == '_' || c == '.' || c == '~')
			return (true);
		return (false);
	}
}

std::string	TemplateRenderer::readFile(const std::string &path)
{
	std::ifstream		file(path.c_str());
	std::ostringstream	buffer;

	if (!file.is_open())
		return ("");
	buffer << file.rdbuf();
	return (buffer.str());
}

void	TemplateRenderer::replaceAll(std::string &text,
	const std::string &from, const std::string &to)
{
	size_t	position;

	if (from.empty())
		return;
	position = 0;
	while ((position = text.find(from, position)) != std::string::npos)
	{
		text.replace(position, from.length(), to);
		position += to.length();
	}
}

std::string	TemplateRenderer::render(const std::string &path,
	const Variables &variables)
{
	Variables::const_iterator	it;
	std::string					body;

	body = readFile(path);
	if (body.empty())
		return ("");
	it = variables.begin();
	while (it != variables.end())
	{
		replaceAll(body, it->first, it->second);
		it++;
	}
	return (body);
}

std::string	TemplateRenderer::htmlEscape(const std::string &text)
{
	std::string	result;
	size_t		index;

	index = 0;
	while (index < text.length())
	{
		if (text[index] == '&')
			result += "&amp;";
		else if (text[index] == '<')
			result += "&lt;";
		else if (text[index] == '>')
			result += "&gt;";
		else if (text[index] == '"')
			result += "&quot;";
		else if (text[index] == '\'')
			result += "&#39;";
		else
			result += text[index];
		index++;
	}
	return (result);
}

std::string	TemplateRenderer::urlEncodePathSegment(const std::string &text)
{
	std::ostringstream	stream;
	size_t				index;
	unsigned char		c;

	index = 0;
	while (index < text.length())
	{
		c = static_cast<unsigned char>(text[index]);
		if (isUrlSafeChar(c))
			stream << text[index];
		else
		{
			stream << '%';
			stream << std::uppercase << std::hex << std::setw(2);
			stream << std::setfill('0') << static_cast<int>(c);
			stream << std::nouppercase << std::dec;
		}
		index++;
	}
	return (stream.str());
}
