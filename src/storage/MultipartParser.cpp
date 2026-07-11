#include "MultipartParser.hpp"

#include <cctype>
#include <map>
#include <sstream>
#include <string>

namespace
{
	/**
	 * @brief Converts a string to lowercase.
	 * @param value - input string
	 * @return lowercase copy of value
	 */
	std::string	toLowerString(const std::string &value)
	{
		std::string	result;
		size_t		index;

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

	/**
	 * @brief Trims leading and trailing spaces and tabs.
	 * @param value - input string
	 * @return trimmed copy of value
	 */
	std::string	trimSpaces(const std::string &value)
	{
		size_t	start;
		size_t	end;

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

	/**
	 * @brief Gets a header value from the request by name.
	 * @param request - source request
	 * @param name - header name
	 * @return trimmed header value or empty string
	 */
	std::string	getHeaderValue(const Request &request, const std::string &name)
	{
		std::map<std::string, std::string>::const_iterator	it;
		std::string									lowerName;

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

	/**
	 * @brief Extracts multipart boundary from Content-Type.
	 * @param contentType - Content-Type header value
	 * @return boundary string or empty string
	 */
	std::string	extractBoundary(const std::string &contentType)
	{
		std::string	lower;
		size_t		start;
		size_t		end;

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
		return (trimSpaces(contentType.substr(start, end - start)));
	}

	/**
	 * @brief Reads a named parameter from a header line.
	 * @param line - header line
	 * @param key - parameter key
	 * @return parameter value or empty string
	 */
	std::string	getLineParameter(const std::string &line,
		const std::string &key)
	{
		std::string	lower;
		std::string	needle;
		size_t		start;
		size_t		end;

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

	/**
	 * @brief Finds a filename in multipart headers.
	 * @param headers - raw multipart headers
	 * @return filename or empty string
	 */
	std::string	extractFileNameFromHeaders(const std::string &headers)
	{
		std::istringstream	stream(headers);
		std::string			line;
		std::string			lower;
		std::string			fileName;

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

	/**
	 * @brief Skips a single CRLF or LF line break.
	 * @param body - multipart body
	 * @param position - current cursor position
	 * @return true if a line break was consumed
	 */
	bool	skipPartLineBreak(const std::string &body, size_t &position)
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

	/**
	 * @brief Locates the end of multipart headers.
	 * @param body - multipart body
	 * @param position - search start position
	 * @param separatorLength - detected separator length
	 * @return index of header terminator or npos
	 */
	size_t	findHeadersEnd(const std::string &body, size_t position,
		size_t &separatorLength)
	{
		size_t	end;

		end = body.find("\r\n\r\n", position);
		separatorLength = 4;
		if (end != std::string::npos)
			return (end);
		end = body.find("\n\n", position);
		separatorLength = 2;
		return (end);
	}

	/**
	 * @brief Finds the next multipart boundary marker.
	 * @param body - multipart body
	 * @param marker - boundary marker
	 * @param position - search start position
	 * @return index of next boundary or npos
	 */
	size_t	findNextBoundary(const std::string &body,
		const std::string &marker, size_t position)
	{
		size_t	next;

		next = body.find("\r\n" + marker, position);
		if (next != std::string::npos)
			return (next);
		next = body.find("\n" + marker, position);
		return (next);
	}

	/**
	 * @brief Creates an invalid uploaded-file record.
	 * @return invalid UploadedFile instance
	 */
	MultipartParser::UploadedFile	makeInvalidUpload(void)
	{
		MultipartParser::UploadedFile	uploaded;

		uploaded.valid = false;
		return (uploaded);
	}
}

	/**
	 * @brief Checks whether the request is multipart/form-data.
	 * @param request - source request
	 * @return true when Content-Type is multipart/form-data
	 */
bool	MultipartParser::isMultipartRequest(const Request &request)
{
	std::string	contentType;

	contentType = toLowerString(getHeaderValue(request, "Content-Type"));
	return (contentType.find("multipart/form-data") != std::string::npos);
}

	/**
	 * @brief Extracts the first uploaded file from a multipart request.
	 * @param request - source request
	 * @return parsed uploaded file or invalid result
	 */
MultipartParser::UploadedFile	MultipartParser::parseFileUpload(
	const Request &request)
{
	UploadedFile	uploaded;
	std::string	contentType;
	std::string	boundary;
	std::string	marker;
	const std::string	&body = request.getBody();
	size_t		position;
	size_t		headersEnd;
	size_t		separatorLength;
	size_t		dataStart;
	size_t		dataEnd;
	std::string	headers;
	std::string	fileName;

	uploaded = makeInvalidUpload();
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
			return (makeInvalidUpload());
		headersEnd = findHeadersEnd(body, position, separatorLength);
		if (headersEnd == std::string::npos)
			return (makeInvalidUpload());
		headers = body.substr(position, headersEnd - position);
		dataStart = headersEnd + separatorLength;
		dataEnd = findNextBoundary(body, marker, dataStart);
		if (dataEnd == std::string::npos)
			return (makeInvalidUpload());
		fileName = extractFileNameFromHeaders(headers);
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
