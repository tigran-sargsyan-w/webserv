#ifndef CHUNKED_DECODER_HPP
# define CHUNKED_DECODER_HPP

# include <cstddef>
# include <string>

enum ChunkedDecodeStatus
{
	CHUNKED_NEED_MORE_DATA,
	CHUNKED_COMPLETED,
	CHUNKED_BAD_REQUEST,
	CHUNKED_BODY_TOO_LARGE
};

class ChunkedDecoder
{
	public:
		static ChunkedDecodeStatus inspect(const std::string &input, std::size_t bodyStart,
			std::size_t maxBodySize, std::size_t &messageEnd);
		static bool decode(const std::string &input, std::size_t bodyStart,
			std::string &decodedBody, std::size_t &messageEnd);

	private:
		static ChunkedDecodeStatus process(const std::string &input, std::size_t bodyStart,
			std::size_t maxBodySize, std::string *decodedBody, std::size_t &messageEnd);
};

#endif