#include <sys/fcntl.h>
#include <unistd.h>
#include "common_headers.h"

epoll_utility::epoll_utility()
{
	epoll_fd = epoll_create(5);
	if (epoll_fd < 0) {
		perror("Epoll failed");
		exit(1);
	}
}

epoll_utility::~epoll_utility()
{
	close(epoll_fd);
}

void epoll_utility::add_fd_or_event_to_epoll(int fd, bool one_shot, bool use_et,int ev)
{
	epoll_event events;
	events.data.fd = fd;

	if (use_et)
		events.events = EPOLLIN | EPOLLET | EPOLLRDHUP | ev;
	else
		events.events = EPOLLIN | EPOLLRDHUP | ev;
	if (one_shot)
		events.events |= EPOLLONESHOT;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &events) < 0) {
		epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &events);
	}
}

int epoll_utility::wait_for_epoll(int timeout)
{
	return epoll_wait(epoll_fd, events, EPOLL_EVENT_NUMBER, timeout);
}

int epoll_utility::set_fd_no_block(int fd)
{
	int old_option = fcntl(fd, F_GETFL);
	int new_option = old_option | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_option);
	return old_option;
}

void epoll_utility::remove_fd_from_epoll(int fd)
{
	epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, 0);
	close(fd);
}
