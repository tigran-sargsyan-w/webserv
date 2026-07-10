#ifndef CGI_SESSION_HPP
# define CGI_SESSION_HPP

# include <cstddef>
# include <ctime>
# include <string>
# include <sys/types.h>

class CgiSession
{
	public:
		CgiSession();

		void	reset();
		bool	hasActiveProcess() const;
		bool	hasActiveFd() const;
		bool	isActive() const;

		pid_t		pid;
		int			stdinFd;
		int			stdoutFd;
		std::string	inputBuffer;
		size_t		inputSent;
		std::string	outputBuffer;
		bool		stdinClosed;
		bool		stdoutClosed;
		bool		finished;
		time_t		startTime;
};

#endif
