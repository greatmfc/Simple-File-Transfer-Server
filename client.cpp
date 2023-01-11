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
	string data_msg = "m/";
	data_msg += msg;
	write(socket_fd, data_msg.c_str(), data_msg.length());
	char code = '0';
	read(socket_fd, &code, sizeof code);
	if (code != '1' && errno != 0) {
		perror("Something wrong with the server");
	}
}

get_file::get_file(setup& s, string_view&& msge) {
	pt = &s;
	socket_fd = pt->socket_fd;
	file_name = msge;
}

void get_file::get_it()
{
	string pre_msg="g/";
	if (file_name.find('/') != string_view::npos) {
		throw std::invalid_argument("File name can not contain path.");
	}
	pre_msg += file_name;
	write(socket_fd, pre_msg.c_str(), pre_msg.length());
	//send a request first
	data_info dis{};
	char tmp_buf[BUFFER_SIZE]{ 0 };
	ssize_t ret = read(socket_fd, tmp_buf, BUFFER_SIZE);
	if (ret <= 1) {
		cerr<<"File might not be found in the server.\n";
		exit(1);
	}
	char code = '1';
	write(socket_fd, &code, sizeof(code));
	//inform the server to send formal data

	char full_path[64]="./";
	const char* msg = tmp_buf;
	ssize_t size = atoi(strchr(msg, '/')+1);
	strncat(full_path, msg, strcspn(msg, "/"));
	int write_fd = open(full_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	dis.bytes_to_deal_with = size;
	dis.buf = new char[size];
	char* buf = dis.buf;
	while (size)
	{
		ret=read(socket_fd, buf, size);
		if (ret <= 0)
		{
			perror("Receieve failed");
			exit(1);
		}
		size -= ret;
		progress_bar((float)(dis.bytes_to_deal_with - size), (float)dis.bytes_to_deal_with);
		buf += ret;
	}
	cout << '\n';
	buf = dis.buf;
	for (;;) {
		ret = write(write_fd, dis.buf, dis.bytes_to_deal_with);
		if (ret < 0) {
#ifdef DEBUG
			perror("Server write to local storage failed");
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
	delete buf;
}

