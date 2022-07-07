#include "common_headers.h"
#include <liburing.h>

class iouring_utility
{
public:
	iouring_utility();
	~iouring_utility();

private:
	void initialize_io_instance();	
	void add_accept(unsigned flags); 
	void add_socket_recv(int fd);
	void add_socket_writev(int fd);
	conn_info cis[CONN_INFO_NUMBER];
	unsigned socket_fd;
	struct sockaddr_in addr;
	socklen_t len;
	data_info dis[DATA_INFO_NUMBER];
	io_uring io_instance;
	io_uring_params input_params;
};

inline void iouring_utility::initialize_io_instance()
{
	memset(&input_params, 0, sizeof(input_params));
	if (io_uring_queue_init_params(IOURING_QUEUE_DEPTH, &io_instance, &input_params) < 0) { //初始化队列深度并根据param设置ring的参数
		perror("io_uring_init_failed:");
		exit(1);
	}
}

inline void iouring_utility::add_accept(unsigned flags)
{
	struct io_uring_sqe *sqe = io_uring_get_sqe(&io_instance);
	conn_info *conn_i = &cis[socket_fd%CONN_INFO_NUMBER];
	if (sqe) {
		io_uring_prep_accept(sqe, socket_fd, (struct sockaddr*)&addr, &len, 0);
		io_uring_sqe_set_flags(sqe, flags);
		conn_i->fd = socket_fd;
		conn_i->type = ACCEPT;
		io_uring_sqe_set_data(sqe, conn_i);
	}
}

inline void iouring_utility::add_socket_recv(int fd)
{
	struct io_uring_sqe *sqe = io_uring_get_sqe(&io_instance);
	conn_info *conn_i = &cis[fd%CONN_INFO_NUMBER];
	if (sqe) {
		io_uring_prep_recv(sqe, fd, dis[fd % DATA_INFO_NUMBER].buffer_for_pre_messsage, BUFFER_SIZE, 0);
		io_uring_sqe_set_flags(sqe, 0);
		conn_i->fd = fd;
		conn_i->type = READ_PRE;
		io_uring_sqe_set_data(sqe, conn_i);
	}
}

inline void iouring_utility::add_socket_writev(int fd)
{
	struct io_uring_sqe *sqe = io_uring_get_sqe(&io_instance);
	conn_info *conn_i = &cis[fd%CONN_INFO_NUMBER];
	if (sqe) {
		io_uring_prep_writev(sqe, fd, &dis[fd%DATA_INFO_NUMBER].iov, 1, 0);
		io_uring_sqe_set_flags(sqe, 0);
		conn_i->fd = fd;
		conn_i->type = WRITE_DATA;
		io_uring_sqe_set_data(sqe, conn_i);
	}
}
