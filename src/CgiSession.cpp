#include "CgiSession.hpp"

CgiSession::CgiSession()
{
	reset();
}

void	CgiSession::reset()
{
	pid = -1;
	stdinFd = -1;
	stdoutFd = -1;
	inputBuffer.clear();
	inputSent = 0;
	outputBuffer.clear();
	stdinClosed = true;
	stdoutClosed = true;
	finished = false;
	startTime = 0;
}

bool	CgiSession::hasActiveProcess() const
{
	return (pid > 0);
}

bool	CgiSession::hasActiveFd() const
{
	return (stdinFd != -1 || stdoutFd != -1);
}

bool	CgiSession::isActive() const
{
	return (hasActiveProcess() || hasActiveFd());
}
