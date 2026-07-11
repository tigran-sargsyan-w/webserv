#include "ErrorResponseHandler.hpp"
#include "StaticFileHandler.hpp"
#include "MimeTypes.hpp"
#include "Utils.hpp"
#include "PathUtils.hpp"
#include "TemplateRenderer.hpp"
#include "UriUtils.hpp"

#include <algorithm>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <vector>

/**
 * @brief Creates an empty static file handler.
 */
StaticFileHandler::StaticFileHandler() {}

/**
 * @brief Copies a static file handler.
 * @param other - Handler to copy.
 */
StaticFileHandler::StaticFileHandler(const StaticFileHandler &other) { (void)other; }

/**
 * @brief Assigns a static file handler.
 * @param other - Handler to assign from.
 * @return Updated handler.
 */
StaticFileHandler &StaticFileHandler::operator=(const StaticFileHandler &other)
{
	(void)other;
	return (*this);
}

/**
 * @brief Destroys the handler.
 */
StaticFileHandler::~StaticFileHandler() {}

/**
 * @brief Removes the route prefix from a cleaned path when needed.
 * @param cleanPath - Normalized request path.
 * @param route - Matched route configuration.
 * @return Path relative to the route root.
 */
static std::string getCleanPathInsideRoute(const std::string &cleanPath, const RouteConfig &route)
{
	if (route.path == "/")
		return (cleanPath);
	if (cleanPath.find(route.path) != 0)
		return (cleanPath);
	return (cleanPath.substr(route.path.length()));
}

/**
 * @brief Checks whether a filesystem path exists.
 * @param path - Filesystem path.
 * @return True if the path exists.
 */
static bool pathExists(const std::string &path)
{
	struct stat pathStat;

	return (stat(path.c_str(), &pathStat) == 0);
}

/**
 * @brief Checks whether a path is a directory.
 * @param path - Filesystem path.
 * @return True if the path is a directory.
 */
static bool isDirectory(const std::string &path)
{
	struct stat pathStat;

	if (stat(path.c_str(), &pathStat) != 0)
		return (false);
	return (S_ISDIR(pathStat.st_mode));
}

/**
 * @brief Checks whether a path is a regular file.
 * @param path - Filesystem path.
 * @return True if the path is a regular file.
 */
static bool isRegularFile(const std::string &path)
{
	struct stat pathStat;

	if (stat(path.c_str(), &pathStat) != 0)
		return (false);
	return (S_ISREG(pathStat.st_mode));
}

/**
 * @brief Builds a file response.
 * @param path - File path to serve.
 * @return HTTP 200 response with file content.
 */
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

/**
 * @brief Orders autoindex entries by type and name.
 * @param left - Left entry.
 * @param right - Right entry.
 * @return True when left should come first.
 */
static bool compareAutoindexEntries(const AutoindexEntry &left, const AutoindexEntry &right)
{
	if (left.isDirectory != right.isDirectory)
		return (left.isDirectory);
	return (left.name < right.name);
}

/**
 * @brief Reads and sorts directory entries for autoindex.
 * @param dir - Open directory stream.
 * @param directoryPath - Directory path.
 * @return Sorted autoindex entries.
 */
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

/**
 * @brief Builds HTML for autoindex entries.
 * @param entries - Directory entries.
 * @param baseUrl - Base URL for links.
 * @return HTML fragment for the listing.
 */
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
		entryUrl = baseUrl + TemplateRenderer::urlEncodePathSegment(entryName);
		if (it->isDirectory)
		{
			entryUrl += "/";
			entryName += "/";
		}
		html += "<a class=\"card autoindex-entry\" href=\"";
		html += TemplateRenderer::htmlEscape(entryUrl);
		html += "\"><span class=\"autoindex-icon\">";
		if (it->isDirectory)
			html += "📁";
		else
			html += "📄";
		html += "</span><span><strong>";
		html += TemplateRenderer::htmlEscape(entryName);
		html += "</strong></span></a>";
		it++;
	}
	html += "</div>";
	return (html);
}

/**
 * @brief Builds a simple fallback autoindex page.
 * @param requestPath - Original request path.
 * @param entries - Directory entries.
 * @param baseUrl - Base URL for links.
 * @return HTML page body.
 */
static std::string buildFallbackAutoindexBody(const std::string &requestPath,
	const std::vector<AutoindexEntry> &entries, const std::string &baseUrl)
{
	std::vector<AutoindexEntry>::const_iterator it;
	std::string body;
	std::string name;

	body = "<html><body>";
	body += "<h1>Index of " + TemplateRenderer::htmlEscape(requestPath) + "</h1>";
	body += "<ul>";
	it = entries.begin();
	while (it != entries.end())
	{
		name = it->name;
		body += "<li><a href=\"";
		body += TemplateRenderer::htmlEscape(baseUrl
			+ TemplateRenderer::urlEncodePathSegment(name));
		if (it->isDirectory)
			body += "/";
		body += "\">";
		body += TemplateRenderer::htmlEscape(name);
		if (it->isDirectory)
			body += "/";
		body += "</a></li>";
		it++;
	}
	body += "</ul>";
	body += "</body></html>";
	return (body);
}

/**
 * @brief Builds the main autoindex body.
 * @param requestPath - Original request path.
 * @param entries - Directory entries.
 * @param baseUrl - Base URL for links.
 * @param server - Server configuration.
 * @return Rendered HTML body.
 */
static std::string buildAutoindexBody(const std::string &requestPath,
	const std::vector<AutoindexEntry> &entries, const std::string &baseUrl,
	const ServerConfig &server)
{
	TemplateRenderer::Variables variables;
	std::string body;
	std::string templatePath;

	templatePath = PathUtils::join(server.root, "autoindex-template.html");
	variables["{{REQUEST_PATH}}"] = TemplateRenderer::htmlEscape(requestPath);
	variables["{{ENTRY_COUNT}}"] = intToString(entries.size());
	variables["{{ENTRIES}}"] = buildAutoindexEntriesHtml(entries, baseUrl);
	body = TemplateRenderer::render(templatePath, variables);
	if (body.empty())
		return (buildFallbackAutoindexBody(requestPath, entries, baseUrl));
	return (body);
}

/**
 * @brief Builds an autoindex response for a directory.
 * @param requestPath - Original request path.
 * @param directoryPath - Filesystem directory path.
 * @param server - Server configuration.
 * @return HTTP response or error response.
 */
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

/**
 * @brief Tries to serve a configured index file.
 * @param fullPath - Directory path.
 * @param indexName - Index file name.
 * @return File response or empty response.
 */
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

/**
 * @brief Tries route and server index files in order.
 * @param fullPath - Directory path.
 * @param route - Matched route configuration.
 * @param server - Server configuration.
 * @return File response or empty response.
 */
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

/**
 * @brief Handles directory requests.
 * @param requestPath - Original request path.
 * @param fullPath - Filesystem directory path.
 * @param route - Matched route configuration.
 * @param server - Server configuration.
 * @return Index, autoindex, or error response.
 */
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

/**
 * @brief Detects path traversal segments.
 * @param path - Decoded request path.
 * @return True when a traversal segment is found.
 */
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

/**
 * @brief Handles static file requests.
 * Serves files, directories, or errors after path validation.
 * @param request - Incoming request.
 * @param route - Matched route configuration.
 * @param server - Server configuration.
 * @return HTTP response for the request.
 */
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
