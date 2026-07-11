#include "CgiPipeIO.hpp"
#include "Logger.hpp"

#include <unistd.h>

/**
 * @brief Check if all request body data has been written to CGI stdin.
 * @param session - CGI session to inspect
 * @return true if input fully sent
 */
bool CgiPipeIO::hasInputFinished(const CgiSession &session)
{
	return (session.inputSent >= session.inputBuffer.size());
}

/**
 * @brief Write pending bytes from session.inputBuffer to CGI stdin.
 * @param session - CGI session containing buffers and fds
 * @return 0 on success, 1 on write error
 */
int CgiPipeIO::writeToStdin(CgiSession &session)
{
	size_t remaining;
	ssize_t bytesWritten;

	remaining = session.inputBuffer.size() - session.inputSent;
	bytesWritten = write(session.stdinFd,
						 session.inputBuffer.c_str() + session.inputSent, remaining);
	if (bytesWritten <= 0)
	{
		Logger::error() << "Failed to write request body to CGI" << std::endl;
		return (1);
	}
	session.inputSent += static_cast<size_t>(bytesWritten);
	return (0);
}

/**
 * @brief Read available data from CGI stdout into the session output buffer.
 * @param session - CGI session containing fds and buffers
 * @return READ_OK on data, READ_EOF on EOF, READ_ERROR on failure
 */
CgiPipeIO::ReadResult CgiPipeIO::readFromStdout(CgiSession &session)
{
	char buffer[4096];
	ssize_t bytesRead;

	bytesRead = read(session.stdoutFd, buffer, sizeof(buffer));
	if (bytesRead < 0)
	{
		Logger::error() << "Failed to read CGI output" << std::endl;
		return (READ_ERROR);
	}
	if (bytesRead == 0)
		return (READ_EOF);
	session.outputBuffer.append(buffer, static_cast<size_t>(bytesRead));
	return (READ_OK);
}
