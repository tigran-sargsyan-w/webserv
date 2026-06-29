#ifndef CGI_PIPE_IO_HPP
# define CGI_PIPE_IO_HPP

# include "CgiSession.hpp"

class CgiPipeIO
{
	public:
		enum ReadResult
		{
			READ_OK,
			READ_EOF,
			READ_ERROR
		};

		static bool		hasInputFinished(const CgiSession &session);
		static int		writeToStdin(CgiSession &session);
		static ReadResult	readFromStdout(CgiSession &session);

	private:
		CgiPipeIO();
};

#endif
