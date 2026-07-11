#include "CgiSession.hpp"

/**
 * @brief Construct a CgiSession and initialize it.
 */
CgiSession::CgiSession()
{
	reset();
}

/**
 * @brief Reset the session to an initial, inactive state.
 */
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

/**
 * @brief Check whether a child process is associated with this session.
 * @return true if a process pid > 0 is present
 */
bool	CgiSession::hasActiveProcess() const
{
	return (pid > 0);
}

/**
 * @brief Check whether the session has any active fds.
 * @return true if stdin or stdout fd is open
 */
bool	CgiSession::hasActiveFd() const
{
	return (stdinFd != -1 || stdoutFd != -1);
}

/**
 * @brief Determine if the session is active (process or fds present).
 * @return true when active
 */
bool	CgiSession::isActive() const
{
	return (hasActiveProcess() || hasActiveFd());
}
