#ifndef POLLMANAGER_HPP
#define POLLMANAGER_HPP

#include <poll.h>
#include <vector>

class PollManager
{
public:
    PollManager();
    PollManager(const PollManager &other);
    ~PollManager();
    PollManager &operator=(const PollManager &other);

    void addFd(int fd, short events);
    void removeFd(int fd);
    void setEvents(int fd, short events);

    std::vector<pollfd> &getFds();
    const std::vector<pollfd> &getFds() const;

private:
    std::vector<pollfd> pollFds;
};

#endif