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
	/**
	 * @brief Builds a fallback 201 response.
	 * @param fileName - stored file name
	 * @return created response with inline HTML body
	 */
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

	/**
	 * @brief Builds a templated 201 response.
	 * @param fileName - stored file name
	 * @param server - current server configuration
	 * @return created response or fallback response if template is missing
	 */
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

	/**
	 * @brief Checks whether a path points to a directory.
	 * @param path - filesystem path
	 * @return true when the path is a directory
	 */
	bool	isDirectory(const std::string &path)
	{
		struct stat	pathStat;

		if (stat(path.c_str(), &pathStat) != 0)
			return (false);
		return (S_ISDIR(pathStat.st_mode));
	}

	/**
	 * @brief Writes uploaded content to disk.
	 * @param content - file content
	 * @param fullPath - destination path
	 * @param fileName - stored file name
	 * @param server - current server configuration
	 * @return created response or error response
	 */
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

	/**
	 * @brief Validates that a file name is safe for storage.
	 * @param fileName - candidate file name
	 * @return true when the file name is safe
	 */
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

	/**
	 * @brief Saves uploaded content to the target directory.
	 * @param directory - destination directory
	 * @param fileName - output file name
	 * @param content - file content
	 * @param server - current server configuration
	 * @return created response or error response
	 */
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
