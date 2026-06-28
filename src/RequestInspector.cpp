#include "RequestInspector.hpp"
#include "ChunkedDecoder.hpp"
#include "HttpMessageUtils.hpp"
#include "utils.hpp"

#include <string>
#include <sstream>

const std::size_t MAX_HEADERS_SIZE = 32768;

enum ContentLengthResult
{
	CL_ABSENT,
	CL_VALID,
	CL_INVALID
};

RequestInspection::RequestInspection()
	: status(EMPTY),
	  headerEnd(std::string::npos),
	  bodyStart(std::string::npos),
	  messageEnd(std::string::npos),
	  isChunked(false),
	  hasContentLength(false),
	  contentLength(0),
	  requestLine()
{}

static RequestInspection finishInspection(RequestInspector &inspector,
	RequestInspection &inspection, InspectRequestStatus status)
{
	inspection.status = status;
	inspector.status = status;
	return (inspection);
}

static ContentLengthResult getContentLength(const std::string &headers, size_t &contentLength)
{
	std::istringstream stream;
	std::string line;
	std::string key;
	std::string value;
	bool found = false;
	size_t parsed = 0;

	stream.str(headers);
	while (std::getline(stream, line))
	{
		if (!HttpMessageUtils::splitHeaderLine(line, key, value))
			continue;
		key = toLowerCase(key);
		HttpMessageUtils::trimLeft(value);

		if (key == "content-length")
		{
			size_t current = 0;
			// invalid CL (letters, signs...)
			if (!HttpMessageUtils::parseSize(value, current))
				return (CL_INVALID);
			// conflicting CLs
			if (found && current != parsed)
				return (CL_INVALID);
			parsed = current;
			found = true;
		}
	}
	if (!found)
		return (CL_ABSENT);
	contentLength = parsed;
	return (CL_VALID);
}

enum TransferEncodingResult
{
	TE_ABSENT,
	TE_CHUNKED,
	TE_UNSUPPORTED,
	TE_INVALID
};

static TransferEncodingResult getTransferEncoding(
	const std::string &headers)
{
	std::istringstream stream;
	std::string line;
	std::string key;
	std::string value;
	bool found;

	stream.str(headers);
	found = false;

	while (std::getline(stream, line))
	{
		if (!HttpMessageUtils::splitHeaderLine(line, key, value))
			continue;
		key = toLowerCase(key);
		HttpMessageUtils::trimHeaderValue(value);

		if (key != "transfer-encoding")
			continue;

		if (found)
			return (TE_INVALID);

		found = true;
		value = toLowerCase(value);

		if (value.empty())
			return (TE_INVALID);

		if (value == "chunked")
			return (TE_CHUNKED);

		return (TE_UNSUPPORTED);
	}

	return (TE_ABSENT);
}

static InspectRequestStatus toInspectStatus(RequestLineStatus status)
{
	if (status == REQUEST_LINE_OK)
		return (COMPLETED);
	if (status == REQUEST_LINE_URI_TOO_LONG)
		return (URI_TOO_LONG);
	if (status == REQUEST_LINE_NOT_IMPLEMENTED)
		return (NOT_IMPLEMENTED);
	return (BAD_REQUEST);
}

void RequestInspector::inspectRequestLine(const std::string &requestLine, RequestLine &parsedLine)
{
	requestLineValid = false;
	if (!parsedLine.parse(requestLine))
	{
		this->status = toInspectStatus(parsedLine.getStatus());
		return;
	}
	requestLineValid = true;
	this->status = COMPLETED;
}

// InspectRequestStatus RequestInspector::inspectRequest(const std::string &rawRequest, size_t maxBodySize)
// {
// 	std::string requestLine;
// 	std::stringstream ss;
// 	size_t headerEnd;
// 	size_t bodyStart;
// 	size_t contentLength;
// 	size_t currentBodySize;
// 	std::string headers;
// 	RequestLine parsedLine;

// 	std::string version;
// 	TransferEncodingResult teResult;
// 	ChunkedDecodeStatus chunkedStatus;
// 	size_t messageEnd;

// 	if (rawRequest.empty())
// 	{
// 		this->status = NEED_MORE_DATA;
// 		return (this->status);
// 	}

// 	ss << rawRequest;
// 	if (!std::getline(ss, requestLine))
// 	{
// 		this->status = NEED_MORE_DATA;
// 		return (this->status);
// 	}

// 	inspectRequestLine(requestLine, parsedLine);
// 	if (this->status != COMPLETED)
// 		return (this->status);

// 	if (!HttpMessageUtils::findHeaderEnd(rawRequest, headerEnd, bodyStart))
// 	{
// 		this->status = NEED_MORE_DATA;
// 		return (this->status);
// 	}

// 	if (headerEnd > MAX_HEADERS_SIZE)
// 	{
// 		this->status = HEADER_TOO_LARGE;
// 		return (this->status);
// 	}

// 	headers = rawRequest.substr(0, headerEnd);
// 	contentLength = 0;

// 	ContentLengthResult clResult;
// 	clResult = getContentLength(headers, contentLength);

// 	teResult = getTransferEncoding(headers);
// 	version = parsedLine.getVersion();

// 	if (clResult == CL_INVALID)
// 	{
// 		this->status = BAD_REQUEST;
// 		return (this->status);
// 	}
// 	if (teResult == TE_INVALID)
// 	{
// 		this->status = BAD_REQUEST;
// 		return (this->status);
// 	}
// 	if (teResult == TE_UNSUPPORTED)
// 	{
// 		this->status = NOT_IMPLEMENTED;
// 		return (this->status);
// 	}
// 	if (version == "HTTP/1.0" && teResult != TE_ABSENT)
// 	{
// 		this->status = BAD_REQUEST;
// 		return (this->status);
// 	}
// 	if (teResult == TE_CHUNKED && clResult == CL_VALID)
// 	{
// 		this->status = BAD_REQUEST;
// 		return (this->status);
// 	}
// 	if (teResult == TE_CHUNKED)
// 	{
// 		chunkedStatus = ChunkedDecoder::inspect(
// 			rawRequest,
// 			bodyStart,
// 			maxBodySize,
// 			messageEnd);
// 		if (chunkedStatus == CHUNKED_NEED_MORE_DATA)
// 		{
// 			this->status = NEED_MORE_DATA;
// 			return (this->status);
// 		}
// 		if (chunkedStatus == CHUNKED_BODY_TOO_LARGE)
// 		{
// 			this->status = REQUEST_TOO_LARGE;
// 			return (this->status);
// 		}
// 		if (chunkedStatus == CHUNKED_BAD_REQUEST)
// 		{
// 			this->status = BAD_REQUEST;
// 			return (this->status);
// 		}

// 		this->status = COMPLETED;
// 		return (this->status);
// 	}

// 	if (clResult == CL_VALID)
// 	{
// 		if (contentLength > maxBodySize)
// 		{
// 			this->status = REQUEST_TOO_LARGE;
// 			return (this->status);
// 		}

// 		currentBodySize = rawRequest.length() - bodyStart;

// 		if (currentBodySize < contentLength)
// 		{
// 			this->status = NEED_MORE_DATA;
// 			return (this->status);
// 		}
// 	}

// 	this->status = COMPLETED;
// 	return (this->status);
// }

RequestInspection RequestInspector::inspectRequest(const std::string &rawRequest,
	size_t maxBodySize)
{
	RequestInspection		inspection;
	std::string				requestLine;
	std::stringstream		ss;
	size_t					contentLength;
	size_t					currentBodySize;
	std::string				headers;
	std::string				version;
	TransferEncodingResult	teResult;
	ChunkedDecodeStatus		chunkedStatus;
	ContentLengthResult		clResult;

	if (rawRequest.empty())
		return (finishInspection(*this, inspection, NEED_MORE_DATA));

	ss << rawRequest;
	if (!std::getline(ss, requestLine))
		return (finishInspection(*this, inspection, NEED_MORE_DATA));

	inspectRequestLine(requestLine, inspection.requestLine);
	if (this->status != COMPLETED)
		return (finishInspection(*this, inspection, this->status));

	if (!HttpMessageUtils::findHeaderEnd(rawRequest,
			inspection.headerEnd,
			inspection.bodyStart))
	{
		return (finishInspection(*this, inspection, NEED_MORE_DATA));
	}

	if (inspection.headerEnd > MAX_HEADERS_SIZE)
		return (finishInspection(*this, inspection, HEADER_TOO_LARGE));

	headers = rawRequest.substr(0, inspection.headerEnd);

	contentLength = 0;
	clResult = getContentLength(headers, contentLength);
	teResult = getTransferEncoding(headers);
	version = inspection.requestLine.getVersion();

	if (clResult == CL_INVALID)
		return (finishInspection(*this, inspection, BAD_REQUEST));

	if (teResult == TE_INVALID)
		return (finishInspection(*this, inspection, BAD_REQUEST));

	if (teResult == TE_UNSUPPORTED)
		return (finishInspection(*this, inspection, NOT_IMPLEMENTED));

	if (version == "HTTP/1.0" && teResult != TE_ABSENT)
		return (finishInspection(*this, inspection, BAD_REQUEST));

	if (teResult == TE_CHUNKED && clResult == CL_VALID)
		return (finishInspection(*this, inspection, BAD_REQUEST));

	if (clResult == CL_VALID)
	{
		inspection.hasContentLength = true;
		inspection.contentLength = contentLength;
	}

	inspection.isChunked = (teResult == TE_CHUNKED);

	if (inspection.isChunked)
	{
		chunkedStatus = ChunkedDecoder::inspect(
			rawRequest,
			inspection.bodyStart,
			maxBodySize,
			inspection.messageEnd);

		if (chunkedStatus == CHUNKED_NEED_MORE_DATA)
			return (finishInspection(*this, inspection, NEED_MORE_DATA));

		if (chunkedStatus == CHUNKED_BODY_TOO_LARGE)
			return (finishInspection(*this, inspection, REQUEST_TOO_LARGE));

		if (chunkedStatus == CHUNKED_BAD_REQUEST)
			return (finishInspection(*this, inspection, BAD_REQUEST));

		return (finishInspection(*this, inspection, COMPLETED));
	}

	if (inspection.hasContentLength)
	{
		if (inspection.contentLength > maxBodySize)
			return (finishInspection(*this, inspection, REQUEST_TOO_LARGE));

		currentBodySize = rawRequest.length() - inspection.bodyStart;
		if (currentBodySize < inspection.contentLength)
			return (finishInspection(*this, inspection, NEED_MORE_DATA));

		inspection.messageEnd = inspection.bodyStart + inspection.contentLength;
		return (finishInspection(*this, inspection, COMPLETED));
	}

	inspection.messageEnd = rawRequest.length();
	return (finishInspection(*this, inspection, COMPLETED));
}
