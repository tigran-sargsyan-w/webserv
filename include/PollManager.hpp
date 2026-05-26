#ifndef POLLMANAGER_HPP
#define POLLMANAGER_HPP

#include <poll.h>
#include <vector>
#include <cstddef>

class PollManager
{
public:
    PollManager();
    PollManager(const PollManager &other);
    ~PollManager();
    PollManager &operator=(const PollManager &other);

    bool empty() const;
    void addFd(int fd, short events);
    void removeFd(int fd);
    void setEvents(int fd, short events);
    size_t size() const;

    std::vector<pollfd> &getFds();
    const std::vector<pollfd> &getFds() const;

private:
    std::vector<pollfd> pollFds;
};

#endif