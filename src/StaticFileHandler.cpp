#include "ErrorResponseBuilder.hpp"
#include "StaticFileHandler.hpp"
#include "MimeTypes.hpp"
#include "utils.hpp"

#include <algorithm>
#include <cctype>
#include <dirent.h>
#include <iomanip>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <vector>

StaticFileHandler::StaticFileHandler() {}

StaticFileHandler::StaticFileHandler(const StaticFileHandler &other) {(void)other;}

StaticFileHandler &StaticFileHandler::operator=(const StaticFileHandler &other)
{
    (void)other;
    return (*this);
}

StaticFileHandler::~StaticFileHandler() {}

static std::string getPathWithoutQuery(const std::string &path)
{
    size_t questionMark;

    questionMark = path.find('?');
    if (questionMark == std::string::npos)
        return (path);
    return (path.substr(0, questionMark));
}

static std::string joinPaths(const std::string &left, const std::string &right)
{
    if (left.empty())
        return (right);
    if (right.empty())
        return (left);
    if (left[left.length() - 1] == '/' && right[0] == '/')
        return (left + right.substr(1));
    if (left[left.length() - 1] != '/' && right[0] != '/')
        return (left + "/" + right);
    return (left + right);
}

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
        if (name != "." && name != "..")
        {
            entryPath = joinPaths(directoryPath, name);
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

static bool isHexDigit(char c)
{
    return ((c >= '0' && c <= '9')
        || (c >= 'a' && c <= 'f')
        || (c >= 'A' && c <= 'F'));
}

static int hexToInt(char c)
{
    if (c >= '0' && c <= '9')
        return (c - '0');
    if (c >= 'a' && c <= 'f')
        return (c - 'a' + 10);
    if (c >= 'A' && c <= 'F')
        return (c - 'A' + 10);
    return (0);
}

static std::string urlDecodePath(const std::string &path)
{
    std::string result;
    size_t i;
    int value;

    i = 0;
    while (i < path.length())
    {
        if (path[i] == '%' && i + 2 < path.length()
            && isHexDigit(path[i + 1]) && isHexDigit(path[i + 2]))
        {
            value = hexToInt(path[i + 1]) * 16 + hexToInt(path[i + 2]);
            result += static_cast<char>(value);
            i += 3;
        }
        else
        {
            result += path[i];
            i++;
        }
    }
    return (result);
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

static Response buildAutoindexResponse(const std::string &requestPath, const std::string &directoryPath)
{
    Response response;
    DIR *dir;
    std::vector<AutoindexEntry> entries;
    std::vector<AutoindexEntry>::const_iterator it;
    std::string body;
    std::string baseUrl;
    std::string name;

    dir = opendir(directoryPath.c_str());
    if (dir == NULL)
        return (ErrorResponseBuilder::build(403, "Forbidden"));

    entries = getSortedDirectoryEntries(dir, directoryPath);
    closedir(dir);

    baseUrl = requestPath;
    if (baseUrl.empty() || baseUrl[baseUrl.length() - 1] != '/')
        baseUrl += "/";

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

    response.setStatusCode(200);
    response.setBody(body);
    response.addHeader("Content-Type", "text/html");
    response.addHeader("Content-Length", intToString(body.length()));
    response.addHeader("Connection", "close");
    return (response);
}

static Response handleDirectoryRequest(const std::string &requestPath, const std::string &fullPath, const RouteConfig &route)
{
    std::string indexPath;

    if (!route.index.empty())
    {
        indexPath = joinPaths(fullPath, route.index);
        if (isRegularFile(indexPath))
            return (buildFileResponse(indexPath));
    }
    if (route.autoindex)
        return (buildAutoindexResponse(requestPath, fullPath));
    return (ErrorResponseBuilder::build(403, "Forbidden"));
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

Response StaticFileHandler::handle(const Request &request, const RouteConfig &route)
{
    std::string cleanPath;
    std::string decodedPath;
    std::string fullPath;

    cleanPath = getPathWithoutQuery(request.getPath());
    decodedPath = urlDecodePath(cleanPath);
    if (hasPathTraversal(decodedPath))
        return (ErrorResponseBuilder::build(403, "Forbidden"));
    fullPath = joinPaths(route.root, getCleanPathInsideRoute(decodedPath, route));

    if (!pathExists(fullPath))
        return (ErrorResponseBuilder::build(404, "Not Found"));

    if (isDirectory(fullPath))
        return (handleDirectoryRequest(cleanPath, fullPath, route));

    if (isRegularFile(fullPath))
        return (buildFileResponse(fullPath));

    return (ErrorResponseBuilder::build(403, "Forbidden"));
}