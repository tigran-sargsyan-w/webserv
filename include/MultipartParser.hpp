#ifndef MULTIPARTPARSER_HPP
# define MULTIPARTPARSER_HPP

# include "Request.hpp"
# include <string>

namespace MultipartParser
{
	struct UploadedFile
	{
		std::string	fileName;
		std::string	content;
		bool		valid;
	};

	bool			isMultipartRequest(const Request &request);
	UploadedFile	parseFileUpload(const Request &request);
}

#endif
