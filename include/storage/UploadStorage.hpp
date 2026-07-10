#ifndef UPLOADSTORAGE_HPP
# define UPLOADSTORAGE_HPP

# include "Config.hpp"
# include "Response.hpp"
# include <string>

namespace UploadStorage
{
	bool		isSafeFileName(const std::string &fileName);
	Response	save(const std::string &directory,
				const std::string &fileName,
				const std::string &content,
				const ServerConfig &server);
}

#endif
