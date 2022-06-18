#pragma once

#include <cstdio>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <cstring>
#include <unistd.h>
#include <cstdlib>
#include <cstdarg>
#include <sys/param.h>
#include <getopt.h>
#include <ctime>
#include <csignal>
#include <cassert>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <sys/sendfile.h>
#include <exception>
#include <iostream>
#include <mutex>
#include <queue>
#include <string_view>
#include <liburing.h>
#include <sys/epoll.h>

#define DEFAULT_PORT 9007
#define IOV_NUM 1
#define VERSION 1.513
#define BUFFER_SIZE 256
#define BACKLOG 1024
#define IOURING_QUEUE_DEPTH 512
#define CONN_INFO_NUMBER 1024
#define DATA_INFO_NUMBER 10240
#define EPOLL_EVENT_NUMBER 1024

using std::cout;
using std::endl;
using std::invalid_argument;
using std::to_string;
using std::mutex;
using std::unique_lock;
using std::queue;
using std::string_view;
using std::move;
using std::forward;
using std::exit;

enum MyEnum
{
	FILE_TYPE,
	MESSAGE_TYPE,
	NONE_TYPE,
	ERROR_TYPE,
	ACCEPT,
	CLOSE,
	READ_PRE,
	READ_MSG,
	READ_DATA,
	WRITE_DATA
};

typedef struct conn_info //存放文件描述符和文件类型
{
	unsigned fd;
	unsigned type;
} conn_info;

typedef struct data_info
{
	unsigned fd;
	ssize_t bytes_to_deal_with;
	char* buf;
	char buffer_for_pre_messsage[BUFFER_SIZE];
	struct iovec iov;
	sockaddr_in address;
} data_info;

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

class log
{
public:
	log();
	~log();
	void submit_missions(string_view&& sv);
	void submit_missions(MyEnum&& type, const struct sockaddr_in& _addr);
	void submit_missions(MyEnum&& type, const struct sockaddr_in& _addr, const char* msg);
	void init_logfile();
	static inline log* get_instance() {
		static log log_object;
		return &log_object;
	}
	inline void flush_all_missions() {
		write_log();
	};
	constexpr void no_logfile() {
		keep_log = false;
	};

private:
	time_t rawtime;
	struct tm* time_info;
	unique_lock<mutex> locker;
	FILE* logfile_fd;
	bool keep_log = true;
	char log_name[64] = { 0 };
	queue<string_view> container;

	void write_log();
};

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

class setup
{
public:
	setup();
	setup(char* ip_addr,int port);
	~setup() = default;
	int socket_fd;
	struct sockaddr_in addr;

private:
	unsigned long target_addr;
	int write_to_sock;
};

class basic_action
{
protected:
	basic_action() = default;
	virtual ~basic_action() = default;
	int socket_fd;
	int status_code;
	setup* pt;
};

class receive_loop : public basic_action
{
public:
	receive_loop() = default;
	receive_loop(setup* s);
	~receive_loop() = default;
	void loop();

private:
	struct sockaddr_in addr;
	socklen_t len;
	data_info dis[DATA_INFO_NUMBER];
	epoll_utility epoll_instance;

	int decide_action(int fd);
	void deal_with_file(int fd);
	void deal_with_mesg(int fd);
	void reset_buffers();
};

class send_file : public basic_action
{
public:
	send_file() = default;
	send_file(setup* s, char* path);
	~send_file() = default;
	void write_to();

private:
	char* file_path;
};

class send_msg : public basic_action
{
public:
	send_msg() = default;
	send_msg(setup* s, char* msg);
	void write_to();
	~send_msg() = default;
private:
	char* msg;
};

#define LOG_MSG(_addr,_msg) log::get_instance()->submit_missions(MESSAGE_TYPE,_addr,_msg)
#define LOG_FILE(_addr,_msg) log::get_instance()->submit_missions(FILE_TYPE,_addr,_msg)
#define LOG_ACCEPT(_addr) log::get_instance()->submit_missions(ACCEPT,_addr,"")
#define LOG_CLOSE(_addr) log::get_instance()->submit_missions(CLOSE,_addr,"")
#define LOG_VOID(_msg) log::get_instance()->submit_missions(_msg)

