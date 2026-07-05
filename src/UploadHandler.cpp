#include "UploadHandler.hpp"
#include "ErrorResponseHandler.hpp"
#include "PathUtils.hpp"
#include "StoragePathResolver.hpp"

#include <cstdio>
#include <cctype>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <sys/stat.h>

struct UploadedFile
{
	std::string fileName;
	std::string content;
	bool valid;
};

static std::string htmlEscape(const std::string &text)
{
	std::string result;
	size_t index;

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

static Response buildCreatedResponse(const std::string &fileName)
{
	Response response;
	std::string body;

	body = "<html><body><h1>Created: File uploaded</h1>";
	if (!fileName.empty())
		body += "<p>Stored as: " + htmlEscape(fileName) + "</p>";
	body += "</body></html>";
	response.setStatusCode(201);
	response.setBody(body);
	return (response);
}

static bool hasExplicitBodyFraming(const Request &request)
{
	const std::map<std::string, std::string> &headers = request.getHeaders();

	return (headers.find("content-length") != headers.end()
		|| headers.find("transfer-encoding") != headers.end());
}

static std::string toLowerString(const std::string &value)
{
	std::string result;
	size_t index;

	result = value;
	index = 0;
	while (index < result.length())
	{
		result[index] = static_cast<char>(std::tolower(
			static_cast<unsigned char>(result[index])));
		index++;
	}
	return (result);
}

static std::string trimSpaces(const std::string &value)
{
	size_t start;
	size_t end;

	start = 0;
	while (start < value.length()
		&& (value[start] == ' ' || value[start] == '\t'))
		start++;
	end = value.length();
	while (end > start
		&& (value[end - 1] == ' ' || value[end - 1] == '\t'))
		end--;
	return (value.substr(start, end - start));
}

static std::string getHeaderValue(const Request &request,
	const std::string &name)
{
	std::map<std::string, std::string>::const_iterator it;
	std::string lowerName;

	lowerName = toLowerString(name);
	it = request.getHeaders().begin();
	while (it != request.getHeaders().end())
	{
		if (toLowerString(it->first) == lowerName)
			return (trimSpaces(it->second));
		it++;
	}
	return ("");
}

static bool isMultipartRequest(const Request &request)
{
	std::string contentType;

	contentType = toLowerString(getHeaderValue(request, "Content-Type"));
	return (contentType.find("multipart/form-data") != std::string::npos);
}

static std::string extractBoundary(const std::string &contentType)
{
	std::string lower;
	size_t start;
	size_t end;
	std::string boundary;

	lower = toLowerString(contentType);
	start = lower.find("boundary=");
	if (start == std::string::npos)
		return ("");
	start += 9;
	while (start < contentType.length()
		&& (contentType[start] == ' ' || contentType[start] == '\t'))
		start++;
	if (start >= contentType.length())
		return ("");
	if (contentType[start] == '"')
	{
		start++;
		end = contentType.find('"', start);
		if (end == std::string::npos)
			return ("");
		return (contentType.substr(start, end - start));
	}
	end = contentType.find(';', start);
	if (end == std::string::npos)
		end = contentType.length();
	boundary = trimSpaces(contentType.substr(start, end - start));
	return (boundary);
}

static std::string getLineParameter(const std::string &line,
	const std::string &key)
{
	std::string lower;
	std::string needle;
	size_t start;
	size_t end;

	lower = toLowerString(line);
	needle = toLowerString(key) + "=";
	start = lower.find(needle);
	if (start == std::string::npos)
		return ("");
	start += needle.length();
	if (start >= line.length())
		return ("");
	if (line[start] == '"')
	{
		start++;
		end = line.find('"', start);
		if (end == std::string::npos)
			return ("");
		return (line.substr(start, end - start));
	}
	end = line.find(';', start);
	if (end == std::string::npos)
		end = line.length();
	return (trimSpaces(line.substr(start, end - start)));
}

static std::string extractFileNameFromPartHeaders(const std::string &headers)
{
	std::istringstream stream(headers);
	std::string line;
	std::string lower;
	std::string fileName;

	while (std::getline(stream, line))
	{
		if (!line.empty() && line[line.length() - 1] == '\r')
			line.erase(line.length() - 1);
		lower = toLowerString(line);
		if (lower.find("content-disposition:") == 0)
		{
			fileName = getLineParameter(line, "filename");
			if (!fileName.empty())
				return (fileName);
		}
	}
	return ("");
}

static bool isSafeUploadFileName(const std::string &fileName)
{
	if (fileName.empty())
		return (false);
	if (fileName == "." || fileName == "..")
		return (false);
	if (fileName.find('\0') != std::string::npos)
		return (false);
	if (fileName.find('/') != std::string::npos)
		return (false);
	if (fileName.find('\\') != std::string::npos)
		return (false);
	return (true);
}

static bool skipPartLineBreak(const std::string &body, size_t &position)
{
	if (position + 1 < body.length()
		&& body.substr(position, 2) == "\r\n")
	{
		position += 2;
		return (true);
	}
	if (position < body.length() && body[position] == '\n')
	{
		position += 1;
		return (true);
	}
	return (false);
}

static size_t findHeadersEnd(const std::string &body, size_t position,
	size_t &separatorLength)
{
	size_t end;

	end = body.find("\r\n\r\n", position);
	separatorLength = 4;
	if (end != std::string::npos)
		return (end);
	end = body.find("\n\n", position);
	separatorLength = 2;
	return (end);
}

static size_t findNextBoundary(const std::string &body,
	const std::string &marker, size_t position)
{
	size_t next;

	next = body.find("\r\n" + marker, position);
	if (next != std::string::npos)
		return (next);
	next = body.find("\n" + marker, position);
	return (next);
}

static UploadedFile parseMultipartUpload(const Request &request)
{
	UploadedFile uploaded;
	std::string contentType;
	std::string boundary;
	std::string marker;
	const std::string &body = request.getBody();
	size_t position;
	size_t headersEnd;
	size_t separatorLength;
	size_t dataStart;
	size_t dataEnd;
	std::string headers;
	std::string fileName;

	uploaded.valid = false;
	contentType = getHeaderValue(request, "Content-Type");
	boundary = extractBoundary(contentType);
	if (boundary.empty())
		return (uploaded);
	marker = "--" + boundary;
	position = body.find(marker);
	while (position != std::string::npos)
	{
		position += marker.length();
		if (position + 1 < body.length()
			&& body.substr(position, 2) == "--")
			return (uploaded);
		if (!skipPartLineBreak(body, position))
			return (uploaded);
		headersEnd = findHeadersEnd(body, position, separatorLength);
		if (headersEnd == std::string::npos)
			return (uploaded);
		headers = body.substr(position, headersEnd - position);
		dataStart = headersEnd + separatorLength;
		dataEnd = findNextBoundary(body, marker, dataStart);
		if (dataEnd == std::string::npos)
			return (uploaded);
		fileName = extractFileNameFromPartHeaders(headers);
		if (!fileName.empty())
		{
			uploaded.fileName = fileName;
			uploaded.content = body.substr(dataStart, dataEnd - dataStart);
			uploaded.valid = true;
			return (uploaded);
		}
		position = body.find(marker, dataEnd);
	}
	return (uploaded);
}

static Response writeUploadedContent(const std::string &content,
	const std::string &fullPath, const std::string &fileName,
	const ServerConfig &server)
{
	std::ofstream file;

	file.open(fullPath.c_str(), std::ios::out | std::ios::binary);
	if (!file.is_open())
		return (ErrorResponseHandler::build(500,
			"Internal Server Error: Could not open file", server));
	file.write(content.data(), content.size());
	file.close();
	if (file.fail())
	{
		std::remove(fullPath.c_str());
		return (ErrorResponseHandler::build(500,
			"Internal Server Error: Write failed", server));
	}
	return (buildCreatedResponse(fileName));
}

static Response handleMultipartUpload(const Request &request,
	const RouteConfig &route, const ServerConfig &server)
{
	UploadedFile uploaded;
	std::string fullPath;
	struct stat pathStat;

	uploaded = parseMultipartUpload(request);
	if (!uploaded.valid || !isSafeUploadFileName(uploaded.fileName))
		return (ErrorResponseHandler::build(400,
			"Bad Request: Invalid multipart file", server));
	fullPath = PathUtils::join(route.uploadStore, uploaded.fileName);
	if (stat(fullPath.c_str(), &pathStat) == 0 && S_ISDIR(pathStat.st_mode))
		return (ErrorResponseHandler::build(409,
			"Conflict: A directory with this name already exists", server));
	return (writeUploadedContent(uploaded.content, fullPath,
		uploaded.fileName, server));
}

static Response handleRawUpload(const Request &request,
	const RouteConfig &route, const ServerConfig &server)
{
	std::string fullPath;
	std::string fileName;
	struct stat pathStat;

	fullPath = StoragePathResolver::resolve(request, route);
	if (fullPath.empty())
		return (ErrorResponseHandler::build(400,
			"Bad Request: Invalid file name", server));
	fileName = PathUtils::getFileName(fullPath);
	if (stat(fullPath.c_str(), &pathStat) == 0 && S_ISDIR(pathStat.st_mode))
		return (ErrorResponseHandler::build(409,
			"Conflict: A directory with this name already exists", server));
	return (writeUploadedContent(request.getBody(), fullPath, fileName, server));
}

Response UploadHandler::handle(const Request &request, const RouteConfig &route,
	const ServerConfig &server)
{
	if (!route.uploadEnable)
		return (ErrorResponseHandler::build(403,
			"Forbidden: Upload is disabled for this route", server));
	if (route.uploadStore.empty())
		return (ErrorResponseHandler::build(500,
			"Internal Server Error: Upload store not configured", server));
	if (request.getBody().empty() && !hasExplicitBodyFraming(request))
		return (ErrorResponseHandler::build(400,
			"Bad request: Empty body", server));
	if (isMultipartRequest(request))
		return (handleMultipartUpload(request, route, server));
	return (handleRawUpload(request, route, server));
}
