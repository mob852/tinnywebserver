#include "epoll.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int create_epoll_fd() {
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1 error");
        exit(EXIT_FAILURE);
    }
    return epoll_fd;
}

void add_fd_to_epoll(int epoll_fd, int fd, bool enable_et) {
    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = EPOLLIN; // 默认监听读事件

    if (enable_et) {
        ev.events |= EPOLLET; // 设置为ET模式
    }

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        perror("epoll_ctl add error");
        exit(EXIT_FAILURE);
    }
}

void modify_fd_in_epoll(int epoll_fd, int fd, uint32_t events) {
    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = events;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
        perror("epoll_ctl mod error");
        close(fd);
    }
}

void delete_fd_from_epoll(int epoll_fd, int fd) {
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL) == -1) {
        perror("epoll_ctl del error");
        close(fd);
    }
}

