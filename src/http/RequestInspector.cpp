#include "RequestInspector.hpp"
#include "ChunkedDecoder.hpp"
#include "HttpMessageUtils.hpp"
#include "Utils.hpp"

#include <string>
#include <sstream>

namespace
{
	enum ContentLengthResult
	{
		CL_ABSENT,
		CL_VALID,
		CL_INVALID
	};

	enum TransferEncodingResult
	{
		TE_ABSENT,
		TE_CHUNKED,
		TE_UNSUPPORTED,
		TE_INVALID
	};

	const std::size_t MAX_HEADERS_SIZE = 32768;
}

/**
 * @brief Create a default request inspection state.
 */
RequestInspection::RequestInspection()
	: status(EMPTY),
	  headerEnd(std::string::npos),
	  bodyStart(std::string::npos),
	  messageEnd(std::string::npos),
	  isChunked(false),
	  hasContentLength(false),
	  contentLength(0),
	  requestLine() {}

/**
 * @brief Update both inspector and inspection status before returning.
 */
static RequestInspection finishInspection(RequestInspector &inspector,
										  RequestInspection &inspection, InspectRequestStatus status)
{
	inspection.status = status;
	inspector.status = status;
	return (inspection);
}

/**
 * @brief Parse Content-Length headers and validate duplicates.
 */
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
			if (!HttpMessageUtils::parseSize(value, current))
				return (CL_INVALID);
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

/**
 * @brief Parse the Transfer-Encoding header.
 */
static TransferEncodingResult getTransferEncoding(const std::string &headers)
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

/**
 * @brief Convert request-line status to inspection status.
 */
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

/**
 * @brief Extract the first line of a raw request.
 */
static bool extractRequestLine(const std::string &rawRequest,
	std::string &requestLine)
{
	std::stringstream ss;

	ss << rawRequest;
	if (!std::getline(ss, requestLine))
		return (false);
	return (true);
}

/**
 * @brief Locate and extract the header section.
 */
static InspectRequestStatus inspectHeaderSection(
	const std::string &rawRequest,
	RequestInspection &inspection,
	std::string &headers)
{
	if (!HttpMessageUtils::findHeaderEnd(rawRequest, inspection.headerEnd,
			inspection.bodyStart))
		return (NEED_MORE_DATA);
	if (inspection.headerEnd > MAX_HEADERS_SIZE)
		return (HEADER_TOO_LARGE);
	headers = rawRequest.substr(0, inspection.headerEnd);
	return (COMPLETED);
}

/**
 * @brief Inspect message framing headers.
 */
static InspectRequestStatus inspectMessageFraming(
	RequestInspection &inspection,
	const std::string &headers)
{
	ContentLengthResult clResult;
	TransferEncodingResult teResult;
	size_t contentLength;
	std::string version;

	contentLength = 0;
	clResult = getContentLength(headers, contentLength);
	teResult = getTransferEncoding(headers);
	version = inspection.requestLine.getVersion();
	if (clResult == CL_INVALID)
		return (BAD_REQUEST);
	if (teResult == TE_INVALID)
		return (BAD_REQUEST);
	if (teResult == TE_UNSUPPORTED)
		return (NOT_IMPLEMENTED);
	if (version == "HTTP/1.0" && teResult != TE_ABSENT)
		return (BAD_REQUEST);
	if (teResult == TE_CHUNKED && clResult == CL_VALID)
		return (BAD_REQUEST);
	if (clResult == CL_VALID)
	{
		inspection.hasContentLength = true;
		inspection.contentLength = contentLength;
	}
	inspection.isChunked = (teResult == TE_CHUNKED);
	return (COMPLETED);
}

/**
 * @brief Inspect a chunked request body.
 */
static InspectRequestStatus inspectChunkedBody(
	const std::string &rawRequest,
	size_t maxBodySize,
	RequestInspection &inspection)
{
	ChunkedDecodeStatus chunkedStatus;

	chunkedStatus = ChunkedDecoder::inspect(rawRequest, inspection.bodyStart,
		maxBodySize, inspection.messageEnd);
	if (chunkedStatus == CHUNKED_NEED_MORE_DATA)
		return (NEED_MORE_DATA);
	if (chunkedStatus == CHUNKED_BODY_TOO_LARGE)
		return (REQUEST_TOO_LARGE);
	if (chunkedStatus == CHUNKED_BAD_REQUEST)
		return (BAD_REQUEST);
	return (COMPLETED);
}

/**
 * @brief Inspect a Content-Length-delimited body.
 */
static InspectRequestStatus inspectContentLengthBody(
	const std::string &rawRequest,
	size_t maxBodySize,
	RequestInspection &inspection)
{
	size_t currentBodySize;

	if (inspection.contentLength > maxBodySize)
		return (REQUEST_TOO_LARGE);
	currentBodySize = rawRequest.length() - inspection.bodyStart;
	if (currentBodySize < inspection.contentLength)
		return (NEED_MORE_DATA);
	inspection.messageEnd = inspection.bodyStart + inspection.contentLength;
	return (COMPLETED);
}

/**
 * @brief Inspect the request body according to framing headers.
 */
static InspectRequestStatus inspectMessageBody(
	const std::string &rawRequest,
	size_t maxBodySize,
	RequestInspection &inspection)
{
	if (inspection.isChunked)
		return (inspectChunkedBody(rawRequest, maxBodySize, inspection));
	if (inspection.hasContentLength)
		return (inspectContentLengthBody(rawRequest, maxBodySize, inspection));
	inspection.messageEnd = rawRequest.length();
	return (COMPLETED);
}

/**
 * @brief Inspect and parse a request line.
 */
void RequestInspector::inspectRequestLine(const std::string &requestLine, RequestLine &parsedLine)
{
	if (!parsedLine.parse(requestLine))
	{
		this->status = toInspectStatus(parsedLine.getStatus());
		return;
	}
	this->status = COMPLETED;
}

/**
 * @brief Inspect a raw HTTP request and determine its boundaries.
 */
RequestInspection RequestInspector::inspectRequest(const std::string &rawRequest, size_t maxBodySize)
{
	RequestInspection inspection;
	std::string requestLine;
	std::string headers;
	InspectRequestStatus result;

	if (rawRequest.empty())
		return (finishInspection(*this, inspection, NEED_MORE_DATA));

	if (!extractRequestLine(rawRequest, requestLine))
		return (finishInspection(*this, inspection, NEED_MORE_DATA));

	inspectRequestLine(requestLine, inspection.requestLine);
	if (this->status != COMPLETED)
		return (finishInspection(*this, inspection, this->status));

	result = inspectHeaderSection(rawRequest, inspection, headers);
	if (result != COMPLETED)
		return (finishInspection(*this, inspection, result));

	result = inspectMessageFraming(inspection, headers);
	if (result != COMPLETED)
		return (finishInspection(*this, inspection, result));

	result = inspectMessageBody(rawRequest, maxBodySize, inspection);
	return (finishInspection(*this, inspection, result));
}
