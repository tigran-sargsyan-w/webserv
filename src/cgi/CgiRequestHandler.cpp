#include "CgiRequestHandler.hpp"
#include "CgiHandler.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include "PathUtils.hpp"
#include "UriUtils.hpp"

#include <cctype>
#include <ctime>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <sys/time.h>
#include <vector>

static bool headerNameEquals(const std::string &left, const std::string &right);
static bool parseCgiStatusCode(const std::string &value, int &statusCode);
static std::string trimHeaderValue(const std::string &value);

/**
 * @brief Parse a single CGI header line and add it to the response.
 * @param response - response to update
 * @param line - header line text ("Name: value")
 */
static void addCgiHeaderToResponse(Response &response, const std::string &line)
{
	size_t colon;
	std::string name;
	std::string value;
	int statusCode;

	colon = line.find(':');
	if (colon == std::string::npos)
		return;
	name = line.substr(0, colon);
	value = trimHeaderValue(line.substr(colon + 1));
	if (name.empty())
		return;
	if (headerNameEquals(name, "Status"))
	{
		if (parseCgiStatusCode(value, statusCode))
			response.setStatusCode(statusCode);
		return;
	}
	response.addHeader(name, value);
}

/**
 * @brief Parse multiple CGI header lines and add them to the response.
 * @param response - response to update
 * @param headers - raw headers block separated by newlines
 */
static void addCgiHeadersToResponse(Response &response, const std::string &headers)
{
	std::istringstream stream;
	std::string line;

	stream.str(headers);
	while (std::getline(stream, line))
	{
		if (!line.empty() && line[line.length() - 1] == '\r')
			line.erase(line.length() - 1);
		if (!line.empty())
			addCgiHeaderToResponse(response, line);
	}
}

/**
 * @brief Parse a CGI "Status" header value into an integer code.
 * @param value - header value string
 * @param statusCode - output integer status code
 * @return true if parsing succeeded
 */
static bool parseCgiStatusCode(const std::string &value, int &statusCode)
{
	if (value.length() < 3)
		return (false);
	if (!std::isdigit(static_cast<unsigned char>(value[0]))
		|| !std::isdigit(static_cast<unsigned char>(value[1]))
		|| !std::isdigit(static_cast<unsigned char>(value[2])))
		return (false);
	if (value.length() > 3 && value[3] != ' ' && value[3] != '\t')
		return (false);
	statusCode = (value[0] - '0') * 100 + (value[1] - '0') * 10
		+ (value[2] - '0');
	if (statusCode < 100 || statusCode > 599)
		return (false);
	return (true);
}

/**
 * @brief Build an HTTP Response object from raw CGI output.
 * @param cgiOutput - full CGI output including headers and body
 * @return constructed Response
 */
Response CgiRequestHandler::buildResponse(const std::string &cgiOutput)
{
	Response response;
	std::string separator;
	size_t bodyStart;
	std::string cgiHeaders;
	std::string cgiBody;

	separator = "\r\n\r\n";
	bodyStart = cgiOutput.find(separator);
	if (bodyStart == std::string::npos)
	{
		separator = "\n\n";
		bodyStart = cgiOutput.find(separator);
	}
	response.setStatusCode(200);
	if (bodyStart != std::string::npos)
	{
		cgiHeaders = cgiOutput.substr(0, bodyStart);
		cgiBody = cgiOutput.substr(bodyStart + separator.length());
		response.setBody(cgiBody);
		addCgiHeadersToResponse(response, cgiHeaders);
	}
	else
	{
		response.setBody(cgiOutput);
		response.addHeader("Content-Type", "text/plain");
	}
	response.addHeader("Content-Length", intToString(response.getBody().length()));
	response.addHeader("Connection", "close");
	return (response);
}

/**
 * @brief Compute the path inside the route for a request path.
 * @param requestPath - original request path (may include query)
 * @param route - route configuration
 * @return path inside the route
 */
static std::string getPathInsideRoute(const std::string &requestPath, const RouteConfig &route)
{
	std::string cleanPath = UriUtils::getPathWithoutQuery(requestPath);

	if (route.path == "/")
		return (cleanPath);

	if (cleanPath.find(route.path) != 0)
		return (cleanPath);

	return (cleanPath.substr(route.path.length()));
}

/**
 * @brief Convert a long to its decimal string representation.
 * @param value - number to convert
 * @return string form of the number
 */
static std::string longToString(long value)
{
	std::ostringstream stream;

	stream << value;
	return (stream.str());
}

/**
 * @brief Get current time as seconds since epoch string.
 * @return time in seconds as string
 */
static std::string getRequestTime(void)
{
	return (longToString(static_cast<long>(std::time(NULL))));
}

/**
 * @brief Get high-resolution current time as a floating string.
 * @return seconds.microseconds as string
 */
static std::string getRequestTimeFloat(void)
{
	struct timeval time;
	std::ostringstream stream;

	if (gettimeofday(&time, NULL) == -1)
		return (getRequestTime() + ".000000");
	stream << time.tv_sec << "."
		   << std::setw(6) << std::setfill('0') << time.tv_usec;
	return (stream.str());
}

/**
 * @brief Check if a found extension is a true boundary in the path.
 * @param path - full path string
 * @param position - position where extension was found
 * @param extension - extension to check
 * @return true if boundary (end or followed by '/')
 */
static bool isCgiExtensionBoundary(const std::string &path, size_t position, const std::string &extension)
{
	size_t end;

	end = position + extension.length();
	if (end == path.length())
		return (true);
	if (path[end] == '/')
		return (true);
	return (false);
}

/**
 * @brief Find a CGI extension position ensuring boundary rules.
 * @param path - path to search
 * @param extension - extension to find
 * @return position or std::string::npos
 */
static size_t findCgiExtensionPosition(const std::string &path, const std::string &extension)
{
	size_t position;

	position = path.find(extension);
	while (position != std::string::npos)
	{
		if (isCgiExtensionBoundary(path, position, extension))
			return (position);
		position = path.find(extension, position + 1);
	}
	return (std::string::npos);
}

/**
 * @brief Resolve whether a request targets a CGI script and fill info.
 * @param request - incoming HTTP request
 * @param route - route configuration with CGI entries
 * @return filled CgiResolvedPath (isCgi=false if not CGI)
 */
static CgiResolvedPath resolveCgiPath(const Request &request, const RouteConfig &route)
{
	CgiResolvedPath info;
	std::string cleanPath;
	size_t position;
	size_t scriptEnd;

	cleanPath = UriUtils::getPathWithoutQuery(request.getPath());
	for (std::vector<CgiConfig>::const_iterator it = route.cgi.begin();
		 it != route.cgi.end(); ++it)
	{
		position = findCgiExtensionPosition(cleanPath, it->extension);
		if (position != std::string::npos)
		{
			scriptEnd = position + it->extension.length();
			info.isCgi = true;
			info.executable = it->executable;
			info.scriptName = cleanPath.substr(0, scriptEnd);
			info.pathInfo = cleanPath.substr(scriptEnd);
			info.scriptPath = PathUtils::join(route.root, getPathInsideRoute(info.scriptName, route));
			if (!info.pathInfo.empty())
				info.pathTranslated = PathUtils::join(route.root, info.pathInfo);
			return (info);
		}
	}
	return (info);
}

/**
 * @brief Trim leading/trailing whitespace and CR from a header value.
 * @param value - raw header value
 * @return trimmed string
 */
static std::string trimHeaderValue(const std::string &value)
{
	size_t start;
	size_t end;

	start = 0;
	while (start < value.length() && (value[start] == ' ' || value[start] == '\t'))
		start++;
	end = value.length();
	while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '\r'))
		end--;
	return (value.substr(start, end - start));
}

/**
 * @brief Retrieve a header value from request case-insensitively.
 * @param request - HTTP request
 * @param name - header name to find
 * @return trimmed header value or empty string
 */
static std::string getHeaderValue(const Request &request, const std::string &name)
{
	std::map<std::string, std::string>::const_iterator it;

	it = request.getHeaders().begin();
	while (it != request.getHeaders().end())
	{
		if (headerNameEquals(it->first, name))
			return (trimHeaderValue(it->second));
		it++;
	}
	return ("");
}

/**
 * @brief Determine content length string for a request.
 * @param request - HTTP request
 * @return content length value or empty string
 */
static std::string getContentLength(const Request &request)
{
	std::string contentLength;

	contentLength = getHeaderValue(request, "Content-Length");
	if (!contentLength.empty())
		return (contentLength);
	if (!request.getBody().empty())
		return (intToString(request.getBody().length()));
	return ("");
}

/**
 * @brief Add common CGI-standard variables to the context.
 * @param context - CGI context to populate
 * @param request - HTTP request
 * @param server - server configuration
 * @param remoteAddr - client remote address
 * @param cgiPath - resolved CGI path info
 */
static void addStandardCgiVariables(CgiContext &context, const Request &request, const ServerConfig &server, const std::string &remoteAddr, const CgiResolvedPath &cgiPath)
{
	context.standard.values["AUTH_TYPE"] = "";
	context.standard.values["CONTENT_LENGTH"] = getContentLength(request);
	context.standard.values["CONTENT_TYPE"] = getHeaderValue(request, "Content-Type");
	context.standard.values["GATEWAY_INTERFACE"] = "CGI/1.1";
	context.standard.values["PATH_INFO"] = cgiPath.pathInfo;
	context.standard.values["PATH_TRANSLATED"] = cgiPath.pathTranslated;
	context.standard.values["QUERY_STRING"] = UriUtils::getQueryString(request.getPath());
	context.standard.values["REMOTE_ADDR"] = remoteAddr;
	context.standard.values["REMOTE_HOST"] = "";
	context.standard.values["REMOTE_IDENT"] = "";
	context.standard.values["REMOTE_USER"] = "";
	context.standard.values["REQUEST_METHOD"] = request.getMethod();
	context.standard.values["SCRIPT_NAME"] = cgiPath.scriptName;
	context.standard.values["SERVER_NAME"] = server.serverName;
	context.standard.values["SERVER_PORT"] = intToString(server.listen.port);
	context.standard.values["SERVER_PROTOCOL"] = request.getVersion();
	context.standard.values["SERVER_SOFTWARE"] = "webserv/1.0";
}

/**
 * @brief Add implementation-specific CGI variables to the context.
 * @param context - CGI context to populate
 * @param request - HTTP request
 * @param route - route configuration
 * @param cgiPath - resolved CGI path info
 */
static void addImplementationCgiVariables(CgiContext &context, const Request &request, const RouteConfig &route, const CgiResolvedPath &cgiPath)
{
	context.implementation.values["SCRIPT_FILENAME"] = cgiPath.scriptPath;
	context.implementation.values["DOCUMENT_ROOT"] = route.root;
	context.implementation.values["REQUEST_URI"] = request.getPath();
	context.implementation.values["REQUEST_SCHEME"] = "http";
	context.implementation.values["HTTPS"] = "off";
	context.implementation.values["SERVER_ADMIN"] = "admin@localhost";
	context.implementation.values["REDIRECT_STATUS"] = "200";
	context.implementation.values["FCGI_ROLE"] = "RESPONDER";
	context.implementation.values["PHP_SELF"] = cgiPath.scriptName + cgiPath.pathInfo;
	context.implementation.values["PATH"] = "/usr/bin:/bin";
	context.implementation.values["PWD"] = PathUtils::getDirectoryName(cgiPath.scriptPath);
	context.implementation.values["REQUEST_TIME"] = getRequestTime();
	context.implementation.values["REQUEST_TIME_FLOAT"] = getRequestTimeFloat();
}

/**
 * @brief Case-insensitive comparison of two header names.
 * @param left - first header name
 * @param right - second header name
 * @return true if equal ignoring case
 */
static bool headerNameEquals(const std::string &left, const std::string &right)
{
	size_t i;

	if (left.length() != right.length())
		return (false);
	i = 0;
	while (i < left.length())
	{
		if (std::tolower(static_cast<unsigned char>(left[i])) != std::tolower(static_cast<unsigned char>(right[i])))
			return (false);
		i++;
	}
	return (true);
}

/**
 * @brief Check whether a header name is a content-related header.
 * @param name - header name to check
 * @return true if it is content-related
 */
static bool isContentHeader(const std::string &name)
{
	if (headerNameEquals(name, "Content-Length"))
		return (true);

	if (headerNameEquals(name, "Content-Type"))
		return (true);

	if (headerNameEquals(name, "Transfer-Encoding"))
		return (true);

	return (false);
}

/**
 * @brief Convert a regular header name to CGI/CGI-like HTTP_* form.
 * @param name - original header name
 * @return transformed header name
 */
static std::string buildCgiHttpHeaderName(const std::string &name)
{
	std::string result;
	size_t i;

	result = "HTTP_";
	i = 0;
	while (i < name.length())
	{
		if (name[i] == '-')
			result += '_';
		else
			result += static_cast<char>(std::toupper(static_cast<unsigned char>(name[i])));
		i++;
	}
	return (result);
}

/**
 * @brief Add non-content HTTP headers to CGI context as HTTP_* variables.
 * @param context - CGI context to populate
 * @param request - HTTP request
 */
static void addHttpHeaderVariables(CgiContext &context, const Request &request)
{
	std::map<std::string, std::string>::const_iterator it;

	it = request.getHeaders().begin();
	while (it != request.getHeaders().end())
	{
		if (!isContentHeader(it->first))
		{
			context.httpHeaders.values[buildCgiHttpHeaderName(it->first)] = trimHeaderValue(it->second);
		}
		it++;
	}
}

/**
 * @brief Print CGI context details to debug log when enabled.
 * @param context - CGI context to print
 */
static void debugPrintCgiContext(const CgiContext &context)
{
	if (!Logger::isDebugEnabled())
		return;
	Logger::debug() << "CGI scriptPath: " << context.scriptPath << std::endl;
	Logger::debug() << "CGI scriptFileName: " << context.scriptFileName << std::endl;
	Logger::debug() << "CGI workingDirectory: " << context.workingDirectory << std::endl;
	Logger::debug() << "Request body for CGI:\n[" << context.requestBody << "]\n";
}

/**
 * @brief Build a full CgiContext for executing a CGI script.
 * @param request - HTTP request
 * @param route - route configuration
 * @param server - server configuration
 * @param remoteAddr - remote client address
 * @param cgiPath - resolved CGI path info
 * @return populated CgiContext
 */
static CgiContext buildCgiContext(const Request &request, const RouteConfig &route, const ServerConfig &server, const std::string &remoteAddr, const CgiResolvedPath &cgiPath)
{
	CgiContext context;

	context.executable = cgiPath.executable;
	context.scriptPath = cgiPath.scriptPath;
	context.scriptFileName = PathUtils::getFileName(cgiPath.scriptPath);
	context.workingDirectory = PathUtils::getDirectoryName(cgiPath.scriptPath);
	context.requestBody = request.getBody();
	debugPrintCgiContext(context);
	addStandardCgiVariables(context, request, server, remoteAddr, cgiPath);
	addHttpHeaderVariables(context, request);
	addImplementationCgiVariables(context, request, route, cgiPath);
	return (context);
}

/**
 * @brief Construct a CgiRequestHandler.
 */
CgiRequestHandler::CgiRequestHandler() {}

/**
 * @brief Copy constructor (no-op).
 */
CgiRequestHandler::CgiRequestHandler(const CgiRequestHandler &other)
{
	(void)other;
}

/**
 * @brief Assignment operator (no-op).
 */
CgiRequestHandler &CgiRequestHandler::operator=(const CgiRequestHandler &other)
{
	(void)other;
	return (*this);
}

/**
 * @brief Destroy the CgiRequestHandler.
 */
CgiRequestHandler::~CgiRequestHandler() {}

/**
 * @brief Determine whether a request should be handled by CGI.
 * @param request - HTTP request
 * @param route - route configuration
 * @return true if the route/request maps to a CGI script
 */
bool CgiRequestHandler::isCgiRequest(const Request &request, const RouteConfig &route)
{
	CgiResolvedPath cgiPath;

	cgiPath = resolveCgiPath(request, route);
	return (cgiPath.isCgi);
}

/**
 * @brief Build a CgiContext for a request if it targets CGI.
 * @param request - HTTP request
 * @param route - route configuration
 * @param server - server configuration
 * @param remoteAddr - client remote address
 * @return populated CgiContext or empty if not CGI
 */
CgiContext CgiRequestHandler::buildContext(const Request &request, const RouteConfig &route, const ServerConfig &server, const std::string &remoteAddr)
{
	CgiResolvedPath cgiPath;

	cgiPath = resolveCgiPath(request, route);
	if (!cgiPath.isCgi)
		return (CgiContext());

	return (buildCgiContext(request, route, server, remoteAddr, cgiPath));
}
