#include "ChunkedDecoder.hpp"

static const std::size_t MAX_CHUNK_SIZE_LINE = 1024;
static const std::size_t MAX_TRAILER_SECTION = 32768;

/**
 * @brief Convert a hexadecimal character to its numeric value.
 */
static int hexDigitValue(char character)
{
	if (character >= '0' && character <= '9')
		return (character - '0');
	if (character >= 'a' && character <= 'f')
		return (character - 'a' + 10);
	if (character >= 'A' && character <= 'F')
		return (character - 'A' + 10);
	return (-1);
}

/**
 * @brief Parse a chunk-size line and extract the chunk length.
 */
static bool parseChunkSize(const std::string &line, std::size_t &chunkSize)
{
	std::string sizeText;
	std::size_t extensionPosition;
	std::size_t maxValue;
	std::size_t index;
	int digit;

	extensionPosition = line.find(';');
	sizeText = line.substr(0, extensionPosition);

	if (sizeText.empty())
		return (false);

	maxValue = static_cast<std::size_t>(-1);
	chunkSize = 0;
	index = 0;

	while (index < sizeText.length())
	{
		digit = hexDigitValue(sizeText[index]);
		if (digit < 0)
			return (false);

		if (chunkSize >
			(maxValue - static_cast<std::size_t>(digit)) / 16)
			return (false);

		chunkSize = chunkSize * 16
			+ static_cast<std::size_t>(digit);

		index++;
	}

	return (true);
}

/**
 * @brief Check whether a trailer line has a valid header-like format.
 */
static bool isValidTrailerLine(const std::string &line)
{
	std::size_t colonPosition;

	colonPosition = line.find(':');

	if (colonPosition == std::string::npos)
		return (false);
	if (colonPosition == 0)
		return (false);

	return (true);
}

/**
 * @brief Process a chunked body and optionally decode it.
 */
ChunkedDecodeStatus ChunkedDecoder::process(const std::string &input, std::size_t bodyStart,
	std::size_t maxBodySize, std::string *decodedBody, std::size_t &messageEnd)
{
	std::size_t position;
	std::size_t lineEnd;
	std::size_t chunkSize;
	std::size_t decodedSize;
	std::size_t trailerStart;
	std::string line;

	messageEnd = std::string::npos;

	if (bodyStart > input.length())
		return (CHUNKED_BAD_REQUEST);

	if (decodedBody != NULL)
		decodedBody->clear();

	position = bodyStart;
	decodedSize = 0;

	while (true)
	{
		lineEnd = input.find("\r\n", position);

		if (lineEnd == std::string::npos)
		{
			if (input.length() - position > MAX_CHUNK_SIZE_LINE)
				return (CHUNKED_BAD_REQUEST);
			return (CHUNKED_NEED_MORE_DATA);
		}

		if (lineEnd - position > MAX_CHUNK_SIZE_LINE)
			return (CHUNKED_BAD_REQUEST);

		line = input.substr(position, lineEnd - position);

		if (!parseChunkSize(line, chunkSize))
			return (CHUNKED_BAD_REQUEST);

		position = lineEnd + 2;

		if (chunkSize == 0)
		{
			trailerStart = position;

			while (true)
			{
				lineEnd = input.find("\r\n", position);

				if (lineEnd == std::string::npos)
				{
					if (input.length() - trailerStart
						> MAX_TRAILER_SECTION)
						return (CHUNKED_BAD_REQUEST);
					return (CHUNKED_NEED_MORE_DATA);
				}

				if (lineEnd - trailerStart
					> MAX_TRAILER_SECTION)
					return (CHUNKED_BAD_REQUEST);

				if (lineEnd == position)
				{
					messageEnd = lineEnd + 2;
					return (CHUNKED_COMPLETED);
				}

				line = input.substr(position, lineEnd - position);

				if (!isValidTrailerLine(line))
					return (CHUNKED_BAD_REQUEST);

				position = lineEnd + 2;
			}
		}

		if (decodedSize > maxBodySize)
			return (CHUNKED_BODY_TOO_LARGE);

		if (chunkSize > maxBodySize - decodedSize)
			return (CHUNKED_BODY_TOO_LARGE);

		if (input.length() - position < chunkSize)
			return (CHUNKED_NEED_MORE_DATA);

		if (decodedBody != NULL)
			decodedBody->append(input, position, chunkSize);

		decodedSize += chunkSize;
		position += chunkSize;

		if (input.length() - position < 2)
			return (CHUNKED_NEED_MORE_DATA);

		if (input.compare(position, 2, "\r\n") != 0)
			return (CHUNKED_BAD_REQUEST);

		position += 2;
	}
}

/**
 * @brief Inspect a chunked body without materializing it.
 */
ChunkedDecodeStatus ChunkedDecoder::inspect(const std::string &input, std::size_t bodyStart,
	std::size_t maxBodySize, std::size_t &messageEnd)
{
	return (process(input, bodyStart, maxBodySize, NULL, messageEnd));
}

/**
 * @brief Decode a complete chunked body into a string.
 */
bool ChunkedDecoder::decode(const std::string &input, std::size_t bodyStart,
	std::string &decodedBody, std::size_t &messageEnd)
{
	std::string result;
	ChunkedDecodeStatus status;

	status = process(input, bodyStart, static_cast<std::size_t>(-1), &result, messageEnd);

	if (status != CHUNKED_COMPLETED)
		return (false);

	decodedBody.swap(result);
	return (true);
}