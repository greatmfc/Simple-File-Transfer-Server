#ifdef ENABLE_MODULES
module;
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <sys/socket.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <exception>
#include <iostream>
#include <string_view>
#include <thread>
#include <sys/epoll.h>
#include <fstream>
#include <unordered_map>
#include <functional>
#include <future>
#include <memory>

#define DEFAULT_PORT 9007
#define VERSION 1.730
#define BUFFER_SIZE 128
#define ALARM_TIME 300

#define LOG_MSG(_addr,_msg) log::get_instance()->submit_missions(MESSAGE_TYPE,_addr,_msg)
#define LOG_FILE(_addr,_msg) log::get_instance()->submit_missions(FILE_TYPE,_addr,_msg)
#define LOG_ACCEPT(_addr) log::get_instance()->submit_missions(ACCEPT,_addr,"")
#define LOG_CLOSE(_addr) log::get_instance()->submit_missions(CLOSE,_addr,"")
#define LOG_ERROR(_addr) log::get_instance()->submit_missions(ERROR_TYPE,_addr,strerror(errno))
#define LOG_VOID(_msg) log::get_instance()->submit_missions(_msg)

export module sft;
import structs;
import thpool;
import epoll_util;
import log;

using std::cout;
using std::endl;
using std::invalid_argument;
using std::to_string;
using std::move;
using std::forward;
using std::exit;
using std::thread;
using std::string;
using std::ios;
using std::ofstream;
using std::string_view;
using std::runtime_error;
using std::cerr;
using std::unordered_map;
using std::bind;
using std::future;
using std::make_shared;
using std::function;
using std::packaged_task;

export{
	class setup
	{
	public:
		setup();
		setup(char* ip_addr, int port);
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
		~basic_action() = default;
		int socket_fd;
		int status_code;
		setup* pt;
	};

	class receive_loop : public basic_action
	{
	public:
		receive_loop() = default;
		receive_loop(setup& s);
		~receive_loop() = default;
		static void stop_loop();
		void loop();

	private:
		struct sockaddr_in addr;
		socklen_t len;
		epoll_utility epoll_instance;
		unordered_map<int, data_info> connection_storage;
		unordered_map<unsigned int, ofstream*> addr_to_stream;
		static inline bool running;
		static inline int pipe_fd[2];

		int decide_action(int fd);
		void deal_with_file(int fd);
		void deal_with_mesg(int fd);
		void deal_with_gps(int fd);
		void deal_with_get_file(int fd);
		void close_connection(int fd);
		int get_prefix(int fd);
		static void alarm_handler(int sig);
	};

	class send_file : public basic_action
	{
	public:
		send_file() = default;
		send_file(setup& s, char*& path);
		~send_file() = default;
		void write_to();

	private:
		char* file_path;
	};

	class send_msg : public basic_action
	{
	public:
		send_msg() = default;
		send_msg(setup& s, char*& msg);
		void write_to();
		~send_msg() = default;
	private:
		char* msg;
	};

	class get_file : public basic_action
	{
	public:
		get_file() = delete;
		get_file(setup& s, string_view&& msg);
		~get_file() = default;
		void get_it();

	private:
		string_view file_name;

	};
}

#else
#include <cstring>
#include <cassert>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <exception>
#include <iostream>
#include "common_headers.h"
using std::cout;
using std::cerr;
using std::endl;
using std::to_string;
using std::invalid_argument;
#endif

send_file::send_file(setup& s,const string& path)
{
	pt = &s;
	file_path = path;
	socket_fd = pt->socket_fd;
}

void send_file::write_to()
{
	int file_fd = open(file_path.data(), O_RDONLY);
	if (file_fd < 0) {
		perror("Open file failed");
		close(socket_fd);
		return;
	}
	struct stat st;
	fstat(file_fd, &st);
	ssize_t ret = 0;
	char *msg = strrchr(file_path.data(), '/') + 1;
	strcat(msg, "/");
	strcat(msg, to_string(st.st_size).data());
	char complete_msg[] = "f/";
	strcat(complete_msg, msg);
	write(socket_fd, complete_msg, strlen(complete_msg));
	char flag = '0';
	ret = recv(socket_fd, &flag, sizeof(flag), 0);
	assert(ret >= 0);
	if (flag != '1' || ret < 0) {
#ifdef DEBUG
		cout << "Receive flag from server failed.\n";
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
	//close(socket_fd);
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
		LOG_ERROR_C(addr);
		perror("Binding failed");
		exit(-1);
	}
}

setup::setup(const char* ip_addr,const int port)
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

send_msg::send_msg(setup& s, const string_view& msg) : msg(msg)
{
	pt = &s;
	socket_fd = pt->socket_fd;
}

void send_msg::write_to()
{
	char pre_msg[128]{ 0 };
	strcpy(pre_msg, "m/");
	strcpy(pre_msg, msg.data());
	write(socket_fd, pre_msg, strlen(pre_msg));
	char code = '0';
	read(socket_fd, &code, sizeof code);
	if (code != '1') {
		perror("Something wrong with the server");
	}
	//close(socket_fd);
}

get_file::get_file(setup& s, string_view&& msge) {
	pt = &s;
	socket_fd = pt->socket_fd;
	file_name = msge;
}

void get_file::get_it()
{
	char pre_msg[128]{ 0 };
	strcpy(pre_msg, "g/");
	strcat(pre_msg, file_name.data());
	write(socket_fd, pre_msg, strlen(pre_msg));
	//send a request first
	data_info dis{};
	memset(&dis, 0, sizeof dis);
	ssize_t ret = read(socket_fd, dis.buffer_for_pre_messsage, sizeof dis.buffer_for_pre_messsage);
	if (ret <= 1) {
		cerr<<"File might not be found in the server.\n";
		exit(1);
	}
	char code = '1';
	write(socket_fd, &code, sizeof(code));
	//inform the server to send formal data

	char full_path[64]="./";
	char* msg = dis.buffer_for_pre_messsage;
	ssize_t size = atoi(strchr(msg, '/')+1);
	strncat(full_path, msg, strcspn(msg, "/"));
	int write_fd = open(full_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	dis.bytes_to_deal_with = size;
	dis.buf = new char[size];
	char* buf = dis.buf;
	while (size)
	{
		ret=read(socket_fd, buf, size);
		if (ret < 0)
		{
			//LOG_ERROR_C(dis.address);
			perror("Receieve failed");
			exit(1);
		}
		size -= ret;
		buf += ret;
	}
	//di->fd = write_fd;
	buf = dis.buf;
	for (;;) {
		ret = write(write_fd, dis.buf, dis.bytes_to_deal_with);
		if (ret < 0) {
			//LOG_ERROR_C(dis.address);
#ifdef DEBUG
			perror("Server write to local failed");
#endif // DEBUG
			exit(1);
		}
		dis.bytes_to_deal_with -= ret;
		dis.buf += ret;
		if (dis.bytes_to_deal_with <= 0) {
			break;
		}
	}
#ifdef DEBUG
	cout << "Success on getting file: " << msg << endl;
#endif // DEBUG
	//LOG_INFO("Fetching file from server:", ADDRSTR(dis.address), ' ', msg);
	//LOG_FILE(dis.address, move(msg));
	delete buf;
	//close(write_fd);
	memset(dis.buffer_for_pre_messsage, 0, BUFFER_SIZE);
}

