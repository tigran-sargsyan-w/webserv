#include "ErrorResponseHandler.hpp"
#include "StaticFileHandler.hpp"
#include "MimeTypes.hpp"
#include "utils.hpp"
#include "PathUtils.hpp"
#include "UriUtils.hpp"

#include <algorithm>
#include <cctype>
#include <dirent.h>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <vector>

StaticFileHandler::StaticFileHandler() {}

StaticFileHandler::StaticFileHandler(const StaticFileHandler &other) { (void)other; }

StaticFileHandler &StaticFileHandler::operator=(const StaticFileHandler &other)
{
	(void)other;
	return (*this);
}

StaticFileHandler::~StaticFileHandler() {}

static std::string getCleanPathInsideRoute(const std::string &cleanPath, const RouteConfig &route)
{
	if (route.path == "/")
		return (cleanPath);
	if (cleanPath.find(route.path) != 0)
		return (cleanPath);
	return (cleanPath.substr(route.path.length()));
}

static bool pathExists(const std::string &path)
{
	struct stat pathStat;

	return (stat(path.c_str(), &pathStat) == 0);
}

static bool isDirectory(const std::string &path)
{
	struct stat pathStat;

	if (stat(path.c_str(), &pathStat) != 0)
		return (false);
	return (S_ISDIR(pathStat.st_mode));
}

static bool isRegularFile(const std::string &path)
{
	struct stat pathStat;

	if (stat(path.c_str(), &pathStat) != 0)
		return (false);
	return (S_ISREG(pathStat.st_mode));
}

static Response buildFileResponse(const std::string &path)
{
	Response response;

	response.setStatusCode(200);
	response.setBodyFromFile(path);
	response.addHeader("Content-Type", MimeTypes::getMimeType(path));
	response.addHeader("Content-Length", intToString(response.getBody().length()));
	response.addHeader("Connection", "close");
	return (response);
}

static bool compareAutoindexEntries(const AutoindexEntry &left, const AutoindexEntry &right)
{
	if (left.isDirectory != right.isDirectory)
		return (left.isDirectory);
	return (left.name < right.name);
}

static std::vector<AutoindexEntry> getSortedDirectoryEntries(DIR *dir, const std::string &directoryPath)
{
	std::vector<AutoindexEntry> entries;
	struct dirent *entry;
	AutoindexEntry autoindexEntry;
	std::string name;
	std::string entryPath;

	entry = readdir(dir);
	while (entry != NULL)
	{
		name = entry->d_name;
		if (name != "." && name != ".." && name != ".gitkeep")
		{
			entryPath = PathUtils::join(directoryPath, name);
			autoindexEntry.name = name;
			autoindexEntry.isDirectory = isDirectory(entryPath);
			entries.push_back(autoindexEntry);
		}
		entry = readdir(dir);
	}
	std::sort(entries.begin(), entries.end(), compareAutoindexEntries);
	return (entries);
}

static std::string htmlEscape(const std::string &text)
{
	std::string result;
	size_t i;

	i = 0;
	while (i < text.length())
	{
		if (text[i] == '&')
			result += "&amp;";
		else if (text[i] == '<')
			result += "&lt;";
		else if (text[i] == '>')
			result += "&gt;";
		else if (text[i] == '"')
			result += "&quot;";
		else if (text[i] == '\'')
			result += "&#39;";
		else
			result += text[i];
		i++;
	}
	return (result);
}

static bool isUrlSafeChar(unsigned char c)
{
	if (std::isalnum(c))
		return (true);
	if (c == '-' || c == '_' || c == '.' || c == '~')
		return (true);
	return (false);
}

static std::string urlEncodePathSegment(const std::string &text)
{
	std::ostringstream stream;
	size_t i;
	unsigned char c;

	i = 0;
	while (i < text.length())
	{
		c = static_cast<unsigned char>(text[i]);
		if (isUrlSafeChar(c))
			stream << text[i];
		else
		{
			stream << '%';
			stream << std::uppercase;
			stream << std::hex;
			stream << std::setw(2);
			stream << std::setfill('0');
			stream << static_cast<int>(c);
			stream << std::nouppercase;
			stream << std::dec;
		}
		i++;
	}
	return (stream.str());
}

static std::string readTemplateFile(const std::string &path)
{
	std::ifstream file(path.c_str());
	std::ostringstream buffer;

	if (!file.is_open())
		return ("");
	buffer << file.rdbuf();
	return (buffer.str());
}

static void replaceAll(std::string &text, const std::string &from,
	const std::string &to)
{
	size_t position;

	if (from.empty())
		return;
	position = 0;
	while ((position = text.find(from, position)) != std::string::npos)
	{
		text.replace(position, from.length(), to);
		position += to.length();
	}
}

static std::string buildAutoindexEntriesHtml(const std::vector<AutoindexEntry> &entries,
	const std::string &baseUrl)
{
	std::vector<AutoindexEntry>::const_iterator it;
	std::string html;
	std::string entryUrl;
	std::string entryName;

	if (entries.empty())
		return ("<p class=\"muted\">This directory is empty.</p>");
	html = "<div class=\"autoindex-list\">";
	it = entries.begin();
	while (it != entries.end())
	{
		entryName = it->name;
		entryUrl = baseUrl + urlEncodePathSegment(entryName);
		if (it->isDirectory)
		{
			entryUrl += "/";
			entryName += "/";
		}
		html += "<a class=\"card autoindex-entry\" href=\"";
		html += htmlEscape(entryUrl);
		html += "\"><span class=\"autoindex-icon\">";
		if (it->isDirectory)
			html += "📁";
		else
			html += "📄";
		html += "</span><span><strong>";
		html += htmlEscape(entryName);
		html += "</strong></span></a>";
		it++;
	}
	html += "</div>";
	return (html);
}

static std::string buildFallbackAutoindexBody(const std::string &requestPath,
	const std::vector<AutoindexEntry> &entries, const std::string &baseUrl)
{
	std::vector<AutoindexEntry>::const_iterator it;
	std::string body;
	std::string name;

	body = "<html><body>";
	body += "<h1>Index of " + htmlEscape(requestPath) + "</h1>";
	body += "<ul>";
	it = entries.begin();
	while (it != entries.end())
	{
		name = it->name;
		body += "<li><a href=\"";
		body += htmlEscape(baseUrl + urlEncodePathSegment(name));
		if (it->isDirectory)
			body += "/";
		body += "\">";
		body += htmlEscape(name);
		if (it->isDirectory)
			body += "/";
		body += "</a></li>";
		it++;
	}
	body += "</ul>";
	body += "</body></html>";
	return (body);
}

static std::string buildAutoindexBody(const std::string &requestPath,
	const std::vector<AutoindexEntry> &entries, const std::string &baseUrl,
	const ServerConfig &server)
{
	std::string body;
	std::string templatePath;

	templatePath = PathUtils::join(server.root, "autoindex-template.html");
	body = readTemplateFile(templatePath);
	if (body.empty())
		return (buildFallbackAutoindexBody(requestPath, entries, baseUrl));
	replaceAll(body, "{{REQUEST_PATH}}", htmlEscape(requestPath));
	replaceAll(body, "{{ENTRY_COUNT}}", intToString(entries.size()));
	replaceAll(body, "{{ENTRIES}}", buildAutoindexEntriesHtml(entries, baseUrl));
	return (body);
}

static Response buildAutoindexResponse(const std::string &requestPath, const std::string &directoryPath, const ServerConfig &server)
{
	Response response;
	DIR *dir;
	std::vector<AutoindexEntry> entries;
	std::string body;
	std::string baseUrl;

	dir = opendir(directoryPath.c_str());
	if (dir == NULL)
		return (ErrorResponseHandler::build(403, "Forbidden", server));

	entries = getSortedDirectoryEntries(dir, directoryPath);
	closedir(dir);

	baseUrl = requestPath;
	if (baseUrl.empty() || baseUrl[baseUrl.length() - 1] != '/')
		baseUrl += "/";

	body = buildAutoindexBody(requestPath, entries, baseUrl, server);
	response.setStatusCode(200);
	response.setBody(body);
	response.addHeader("Content-Type", "text/html");
	response.addHeader("Content-Length", intToString(body.length()));
	response.addHeader("Connection", "close");
	return (response);
}

static Response tryServeDirectoryIndex(const std::string &fullPath,
	const std::string &indexName)
{
	std::string indexPath;
	if (indexName.empty())
		return (Response());
	indexPath = PathUtils::join(fullPath, indexName);
	if (!isRegularFile(indexPath))
		return (Response());
	return (buildFileResponse(indexPath));
}

static Response tryServeConfiguredIndexFiles(const std::string &fullPath,
	const RouteConfig &route, const ServerConfig &server)
{
	Response response;
	response = tryServeDirectoryIndex(fullPath, route.index);
	if (response.getStatusCode() != 0)
		return (response);
	response = tryServeDirectoryIndex(fullPath, server.index);
	if (response.getStatusCode() != 0)
		return (response);
	return (Response());
}

static Response handleDirectoryRequest(const std::string &requestPath, const std::string &fullPath, const RouteConfig &route, const ServerConfig &server)
{
	Response response;

	response = tryServeConfiguredIndexFiles(fullPath, route, server);
	if (response.getStatusCode() != 0)
		return (response);
	if (route.autoindex)
		return (buildAutoindexResponse(requestPath, fullPath, server));
	return (ErrorResponseHandler::build(403, "Forbidden", server));
}

static bool hasPathTraversal(const std::string &path)
{
	size_t start;
	size_t slash;
	std::string segment;

	start = 0;
	while (start <= path.length())
	{
		slash = path.find('/', start);
		if (slash == std::string::npos)
			segment = path.substr(start);
		else
			segment = path.substr(start, slash - start);

		if (segment == "..")
			return (true);

		if (slash == std::string::npos)
			break;
		start = slash + 1;
	}
	return (false);
}

Response StaticFileHandler::handle(const Request &request, const RouteConfig &route, const ServerConfig &server)
{
	std::string cleanPath;
	std::string decodedPath;
	std::string fullPath;

	cleanPath = UriUtils::getPathWithoutQuery(request.getPath());
	decodedPath = UriUtils::decodePath(cleanPath);
	if (hasPathTraversal(decodedPath))
		return (ErrorResponseHandler::build(403, "Forbidden", server));
	fullPath = PathUtils::join(route.root, getCleanPathInsideRoute(decodedPath, route));

	if (!pathExists(fullPath))
		return (ErrorResponseHandler::build(404, "Not Found", server));

	if (isDirectory(fullPath))
		return (handleDirectoryRequest(cleanPath, fullPath, route, server));

	if (isRegularFile(fullPath))
		return (buildFileResponse(fullPath));

	return (ErrorResponseHandler::build(403, "Forbidden", server));
}
