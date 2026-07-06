#include "UploadStorage.hpp"
#include "ErrorResponseHandler.hpp"
#include "PathUtils.hpp"
#include "TemplateRenderer.hpp"

#include <cstdio>
#include <fstream>
#include <string>
#include <sys/stat.h>

namespace
{
	Response	buildFallbackCreatedResponse(const std::string &fileName)
	{
		Response	response;
		std::string	body;

		body = "<html><body><h1>Created: File uploaded</h1>";
		body += "<p>Stored as: " + TemplateRenderer::htmlEscape(fileName);
		body += "</p></body></html>";
		response.setStatusCode(201);
		response.setBody(body);
		response.addHeader("Content-Type", "text/html");
		return (response);
	}

	Response	buildCreatedResponse(const std::string &fileName,
		const ServerConfig &server)
	{
		Response	response;
		TemplateRenderer::Variables	variables;
		std::string	body;
		std::string	fileHref;
		std::string	templatePath;

		templatePath = PathUtils::join(server.root, "upload-success.html");
		fileHref = "/uploads/" + TemplateRenderer::urlEncodePathSegment(fileName);
		variables["{{FILENAME}}"] = TemplateRenderer::htmlEscape(fileName);
		variables["{{FILE_HREF}}"] = TemplateRenderer::htmlEscape(fileHref);
		body = TemplateRenderer::render(templatePath, variables);
		if (body.empty())
			return (buildFallbackCreatedResponse(fileName));
		response.setStatusCode(201);
		response.setBody(body);
		response.addHeader("Content-Type", "text/html");
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
		return (buildCreatedResponse(fileName, server));
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
