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
#include <rpc/types.h>
#include <getopt.h>
#include <ctime>
#include <csignal>
#include <exception>
#include <iostream>
#include <cassert>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <sys/sendfile.h>

#define DEFAULT_PORT 9007
#define IOV_NUM 1
#define VERSION 1.513
#define BUFFER_SIZE 256
using namespace std;

typedef struct data_info
{
	unsigned fd;
	ssize_t bytes_to_deal_with;
	char* buf;
	struct iovec iov;
} data_info;

enum MyEnum
{
	FILE_TYPE,
	MESSAGE_TYPE,
	NONE_TYPE
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
	int pre_action(int fd);
	unsigned socket_fd;
	data_info* di;
	int status_code;
	char pre_msg[BUFFER_SIZE];
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
	int decide_action();
	void deal_with_file();
	void deal_with_mesg();
	void reset_buffers();
	int accepted_fd;
	struct sockaddr_in addr;
	socklen_t len;
	char buffer[BUFFER_SIZE];
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

receive_loop::receive_loop(setup* s)
{
	pt = s;
	addr = pt->addr;
	len = sizeof(addr);
	socket_fd = pt->socket_fd;
}

void receive_loop::loop()
{
	int ret = listen(socket_fd, 5);
	assert(ret >= 0);
	while (1) {
		accepted_fd = accept(socket_fd, (struct sockaddr*)&addr, &len);
		if (accepted_fd < 0) {
			perror("Accept failed");
			exit(1);
		}
		pid_t pid = fork();
		if (pid < 0) {
			perror("Fork failed");
			goto child_process;
		}
		else if(pid == 0) {
			child_process:reset_buffers();
			status_code = decide_action();
			if (status_code == FILE_TYPE) {
				deal_with_file();
			}
			else if (status_code == MESSAGE_TYPE) {
				deal_with_mesg();
			}
			exit(0);
		}
	}
}

int receive_loop::decide_action()
{
	char msg1 = '1';
	ssize_t ret = 0;
	ret = read(accepted_fd, pre_msg, BUFFER_SIZE);
	if (ret <= 0) {
		perror("Something happened while read from client");
		close(accepted_fd);
		goto end;
	}
#ifdef DEBUG
	cout << "Read msg from client: " << pre_msg << endl;
#endif // DEBUG
	write(accepted_fd, &msg1, sizeof(msg1));
	switch (pre_msg[0])
	{
	case 'm':return MESSAGE_TYPE;
	case 'f':return FILE_TYPE;
	default:return NONE_TYPE;
	}
	end:return -1;
}

void receive_loop::deal_with_file()
{
	char full_path[64]="./";
	char* msg = pre_msg;
	msg += 2;
	ssize_t size = atoi(strchr(msg, '/')+1);
	strncat(full_path, msg, strcspn(msg, "/"));
	int write_fd = open(full_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	di = new data_info();
	di->bytes_to_deal_with = size;
	di->fd = accepted_fd;
	di->buf = new char[size];
	char* buf = di->buf;
	ssize_t ret = 0;
	while (size)
	{
		ret=read(accepted_fd, buf, size);
		if (ret < 0)
		{
			perror("Receieve failed");
			exit(1);
		}
		size -= ret;
		buf += ret;
	}
	di->fd = write_fd;
	buf = di->buf;
	for (;;) {
		ret = write(write_fd, di->buf, di->bytes_to_deal_with);
		if (ret < 0) {
			perror("Server write to local failed");
			exit(1);
		}
		di->bytes_to_deal_with -= ret;
		di->buf += ret;
		if (di->bytes_to_deal_with <= 0) {
			break;
		}
	}
	cout << "Success on receiving file: " << msg << endl;
	delete buf;
	delete di;
	close(write_fd);
	close(accepted_fd);
}

void receive_loop::deal_with_mesg()
{
	char* pt = pre_msg;
	strcpy(buffer, pt+2);
	strcat(buffer, "\n");
	time_t rawtime;
	struct tm* time_info;
	char log_name[64];
	time(&rawtime);
	time_info = localtime(&rawtime);
	strftime(log_name, 64, "./%Y-%m-%d", time_info);
	int log_fd = open(log_name, O_CREAT | O_APPEND | O_RDWR, 0644);
	write(log_fd, buffer, strlen(buffer));
	close(log_fd);
	close(accepted_fd);
	cout << "Success on receiving message: " << buffer;
}

void receive_loop::reset_buffers()
{
	memset(pre_msg, 0, sizeof pre_msg);
	memset(buffer, 0, sizeof buffer);
}

send_file::send_file(setup* s, char* path)
{
	pt = s;
	file_path = path;
	socket_fd = pt->socket_fd;
}

void send_file::write_to()
{
	int file_fd = open(file_path, O_RDONLY);
	if (file_fd < 0) {
		perror("Open file failed");
		exit(1);
	}
	struct stat st;
	fstat(file_fd, &st);
	ssize_t ret = 0;
	char *msg = strrchr(file_path, '/') + 1;
	strcat(msg, "/");
	strcat(msg, to_string(st.st_size).data());
	char complete_msg[] = "f/";
	strcat(complete_msg, msg);
	write(socket_fd, complete_msg, strlen(complete_msg));
	char flag = '0';
	ret = recv(socket_fd, &flag, sizeof(flag), 0);
	assert(ret >= 0);
	if (flag != '1') {
		printf("Receive flag from server failed.\n");
#ifdef DEBUG
		cout << __LINE__ << endl;
#endif // DEBUG
		exit(1);
	}
	off_t off = 0;
	long send_size = st.st_size;
	while (send_size > 0) {
		ssize_t ret = sendfile(socket_fd, file_fd, &off, send_size);
		if(ret < 0)
		{
			perror("Sendfile failed");
			break;
		}
	#ifdef DEBUG
		cout << "have send :" << ret << endl;
	#endif // DEBUG
		send_size -= ret;
	}
	close(socket_fd);
	close(file_fd);
}

setup::setup()
{
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY); 
	addr.sin_port = htons(DEFAULT_PORT);
	socket_fd = socket(PF_INET, SOCK_STREAM, 0);
	int flag = 1;
	setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
	int ret = bind(socket_fd, (struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0) {
		perror("Binding failed");
		exit(-1);
	}
}

setup::setup(char* ip_addr,int port)
{
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	try
	{
		target_addr = inet_addr(ip_addr);
		if (target_addr == INADDR_NONE) {
			throw invalid_argument("Invalid address:");
		}
	}
	catch (const std::exception& ex)
	{
		cout << ex.what() << ip_addr << endl;
		exit(1);
	}
	memcpy(&addr.sin_addr, &target_addr, sizeof(target_addr));
	unsigned short pt = static_cast<unsigned short>(port);
	addr.sin_port = htons(pt);
	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	int ret = connect(socket_fd, (struct sockaddr*)&addr, sizeof(addr));
	if (ret < 0) {
		perror("Connect failed");
		exit(1);
	}
}

send_msg::send_msg(setup* s, char* msg) : msg(msg)
{
	pt = s;
	socket_fd = pt->socket_fd;
}

void send_msg::write_to()
{
	strcpy(pre_msg, "m/");
	strcat(pre_msg, msg);
	write(socket_fd, pre_msg, strlen(pre_msg));
	close(socket_fd);
}

int basic_action::pre_action(int fd)
{
	char msg1 = '1';
	ssize_t ret = 0;
	ret = read(fd, pre_msg, BUFFER_SIZE);
	assert(ret >= 0);
#ifdef DEBUG
	cout << "Read msg from client: " << pre_msg << endl;
#endif // DEBUG
	write(fd, &msg1, sizeof(msg1));
	switch (pre_msg[0])
	{
	case 'm':return MESSAGE_TYPE;
	case 'f':return FILE_TYPE;
	default:return NONE_TYPE;
	}
	return 0;
}
