#include "CgiPipeIO.hpp"

#include <iostream>
#include <unistd.h>

bool	CgiPipeIO::hasInputFinished(const CgiSession &session)
{
	return (session.inputSent >= session.inputBuffer.size());
}

int	CgiPipeIO::writeToStdin(CgiSession &session)
{
	size_t	remaining;
	ssize_t	bytesWritten;

	remaining = session.inputBuffer.size() - session.inputSent;
	bytesWritten = write(session.stdinFd,
		session.inputBuffer.c_str() + session.inputSent, remaining);
	if (bytesWritten <= 0)
	{
		std::cerr << "Failed to write request body to CGI" << std::endl;
		return (1);
	}
	session.inputSent += static_cast<size_t>(bytesWritten);
	return (0);
}

CgiPipeIO::ReadResult	CgiPipeIO::readFromStdout(CgiSession &session)
{
	char	buffer[4096];
	ssize_t	bytesRead;

	bytesRead = read(session.stdoutFd, buffer, sizeof(buffer));
	if (bytesRead < 0)
	{
		std::cerr << "Failed to read CGI output" << std::endl;
		return (READ_ERROR);
	}
	if (bytesRead == 0)
		return (READ_EOF);
	session.outputBuffer.append(buffer, static_cast<size_t>(bytesRead));
	return (READ_OK);
}
