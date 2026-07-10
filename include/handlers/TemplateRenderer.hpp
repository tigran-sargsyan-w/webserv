#ifndef TEMPLATERENDERER_HPP
# define TEMPLATERENDERER_HPP

# include <map>
# include <string>

namespace TemplateRenderer
{
	typedef std::map<std::string, std::string> Variables;

	std::string	readFile(const std::string &path);
	void		replaceAll(std::string &text, const std::string &from,
				const std::string &to);
	std::string	render(const std::string &path, const Variables &variables);
	std::string	htmlEscape(const std::string &text);
	std::string	urlEncodePathSegment(const std::string &text);
}

#endif
