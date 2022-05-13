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
#define VERSION 1.416
#define BUFFER_SIZE 1024
#define MSG_LEN 1024
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
	char pre_msg[MSG_LEN];
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

