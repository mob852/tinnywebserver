#ifndef EPOLL_H
#define EPOLL_H

#include <sys/epoll.h>

#define MAX_EVENTS 1

int create_epoll_fd();
void add_fd_to_epoll(int epoll_fd, int fd, bool enable_et);
void modify_fd_in_epoll(int epoll_fd, int fd, uint32_t events);
void delete_fd_from_epoll(int epoll_fd, int fd);

#endif // EPOLL_H
