#include "UploadStorage.hpp"
#include "ErrorResponseHandler.hpp"
#include "PathUtils.hpp"

#include <cstdio>
#include <fstream>
#include <string>
#include <sys/stat.h>

namespace
{
	std::string	htmlEscape(const std::string &text)
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

	Response	buildCreatedResponse(const std::string &fileName)
	{
		Response	response;
		std::string	body;

		body = "<html><body><h1>Created: File uploaded</h1>";
		if (!fileName.empty())
			body += "<p>Stored as: " + htmlEscape(fileName) + "</p>";
		body += "</body></html>";
		response.setStatusCode(201);
		response.setBody(body);
		return (response);
	}

	bool	isDirectory(const std::string &path)
	{
		struct stat	pathStat;

		if (stat(path.c_str(), &pathStat) != 0)
			return (false);
		return (S_ISDIR(pathStat.st_mode));
	}

	Response	writeContentToFile(const std::string &content,
		const std::string &fullPath, const std::string &fileName,
		const ServerConfig &server)
	{
		std::ofstream	file;

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
}

bool	UploadStorage::isSafeFileName(const std::string &fileName)
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

Response	UploadStorage::save(const std::string &directory,
	const std::string &fileName, const std::string &content,
	const ServerConfig &server)
{
	std::string	fullPath;

	if (!isSafeFileName(fileName))
		return (ErrorResponseHandler::build(400,
			"Bad Request: Invalid file name", server));
	fullPath = PathUtils::join(directory, fileName);
	if (isDirectory(fullPath))
		return (ErrorResponseHandler::build(409,
			"Conflict: A directory with this name already exists", server));
	return (writeContentToFile(content, fullPath, fileName, server));
}
