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

	std::string	buildUploadedHref(const std::string &fileName)
	{
		return ("/uploads/" + htmlEscape(fileName));
	}

	Response	buildCreatedResponse(const std::string &fileName)
	{
		Response	response;
		std::string	body;
		std::string	safeName;
		std::string	fileHref;

		safeName = htmlEscape(fileName);
		fileHref = buildUploadedHref(fileName);
		body = "<!DOCTYPE html>";
		body += "<html lang=\"en\"><head><meta charset=\"utf-8\">";
		body += "<meta name=\"viewport\" content=\"width=device-width,";
		body += " initial-scale=1\">";
		body += "<title>Upload complete · webserv</title>";
		body += "<link rel=\"stylesheet\" href=\"/css/style.css\">";
		body += "</head><body>";
		body += "<header class=\"nav\"><div class=\"nav-inner\">";
		body += "<a href=\"/index.html\" class=\"brand\">";
		body += "<span class=\"brand-dot\"></span>";
		body += "<span><span class=\"brand-prompt\">~/</span>webserv</span>";
		body += "</a><nav class=\"nav-links\" aria-label=\"Primary\">";
		body += "<a href=\"/index.html\">Home</a>";
		body += "<a href=\"/upload.html\">Upload</a>";
		body += "<a href=\"/uploads/\">Uploads</a>";
		body += "<a href=\"/delete.html\">DELETE</a>";
		body += "</nav></div></header>";
		body += "<main class=\"fade-in\"><section class=\"container\">";
		body += "<span class=\"eyebrow\">// 201 Created</span>";
		body += "<h1>File uploaded successfully</h1>";
		body += "<p class=\"lead\" style=\"color:var(--muted)\">";
		body += "The server accepted the multipart request and stored the";
		body += " file in the configured upload directory.</p>";
		body += "</section><section class=\"section container\">";
		body += "<div class=\"card\"><h3>Stored file</h3>";
		body += "<p><code>" + safeName + "</code></p>";
		body += "<p style=\"color:var(--muted)\">";
		body += "You can now open it, inspect the uploads directory,";
		body += " or upload another file.</p>";
		body += "<p style=\"margin-top:1rem\">";
		body += "<a class=\"btn btn-primary\" href=\"" + fileHref;
		body += "\">Open uploaded file</a> ";
		body += "<a class=\"btn\" href=\"/uploads/\">Open uploads</a> ";
		body += "<a class=\"btn\" href=\"/upload.html\">Upload another</a>";
		body += "</p></div></section></main>";
		body += "<footer><div class=\"container\">";
		body += "<span>webserv</span><span class=\"sep\">/</span>";
		body += "<span>42 School HTTP Server Project</span>";
		body += "</div></footer></body></html>";
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
