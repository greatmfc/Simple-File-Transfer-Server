#if __cplusplus > 201703L
module;
#include <sys/epoll.h>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#define EPOLL_EVENT_NUMBER 1024
export module epoll_util;
export{
	class epoll_utility
	{
	public:
		epoll_utility();
		~epoll_utility();
		void add_fd_or_event_to_epoll(int fd, bool one_shot, bool use_et, int ev);
		int wait_for_epoll();
		int set_fd_no_block(int fd);
		void remove_fd_from_epoll(int fd);
		epoll_event events[EPOLL_EVENT_NUMBER];
	private:
		int epoll_fd;
	};
}
using std::exit;
#else
#include "common_headers.h"
#endif // __cplusplus > 201703L

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

int epoll_utility::wait_for_epoll()
{
	return epoll_wait(epoll_fd,events,EPOLL_EVENT_NUMBER,-1);
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
