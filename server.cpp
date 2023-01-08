#include <cstring>
#include <cstdlib>
#include <cassert>
#include <sys/socket.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <iostream>
#include <string_view>
#include <unordered_map>
#include "common_headers.h"
using std::cout;
using std::endl;
using std::to_string;
using std::ios;

receive_loop::receive_loop(setup& s)
{
	pt = &s;
	len = sizeof(addr);
	socket_fd = pt->socket_fd;
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipe_fd) < 0) {
		LOG_ERROR_C(pt->addr);
		exit(1);
	}
}

void receive_loop::stop_loop()
{
	running = false;
	char msg = '1';
	send(pipe_fd[1], &msg, 1, 0);
}

void receive_loop::loop()
{
	running = true;
	int ret=listen(socket_fd, 5);
	assert(ret >= 0);
	epoll_instance.add_fd_or_event_to_epoll(socket_fd,false,true,0);
	epoll_instance.add_fd_or_event_to_epoll(pipe_fd[0], false, true, 0);
	epoll_instance.set_fd_no_block(socket_fd);
	signal(SIGALRM, alarm_handler);
	alarm(ALARM_TIME);
	LOG_INFO("Server starts.");
#ifdef DEBUG
	auto id = std::this_thread::get_id();
	LOG_DEBUG("The id of main thread is: ", to_string(GETCURID(id)));
#endif // DEBUG
	thread_pool tp;
	tp.init_pool();
	while (running) {
		int count = epoll_instance.wait_for_epoll();
		if (count < 0 && errno != EINTR) {
			LOG_ERROR_C(pt->addr);
			perror("Error epoll_wait");
			exit(1);
		}
		for (int i = 0; i < count; ++i) {
			int react_fd = epoll_instance.events[i].data.fd;

			if (react_fd == socket_fd) {
				while (true)
				{
					int accepted_fd = accept(socket_fd, (struct sockaddr*)&addr, &len);
					if (accepted_fd < 0) {
						if (errno != EAGAIN) {
							LOG_ERROR_C(addr);
							perror("Accept failed");
							exit(1);
						}
						else {
							break;
						}
					}
				#ifdef DEBUG
					cout << "Accept from client:" << inet_ntoa(addr.sin_addr) << endl;
				#endif // DEBUG
					LOG_ACCEPT(addr);
					epoll_instance.add_fd_or_event_to_epoll(accepted_fd, false, true, 0);
					connection_storage[accepted_fd].address = addr;
				}
			}
			else if (epoll_instance.events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
			#ifdef DEBUG
				cout << "Disconnect from client:" << inet_ntoa(connection_storage[react_fd].address.sin_addr) << endl;
			#endif // DEBUG
				//LOG_INFO("Closed by: ",ADDRSTR(connection_storage[react_fd].address.sin_addr));
				LOG_CLOSE(connection_storage[react_fd].address);
				epoll_instance.remove_fd_from_epoll(react_fd);
				close_connection(react_fd);
			}
			else if (react_fd == pipe_fd[0]) {
				char signal='2';
				recv(pipe_fd[0], &signal, sizeof signal, 0);
				if (signal == '1') break;
				LOG_INFO("Tick.");
				//LOG_VOID("Alarm.");
				alarm(ALARM_TIME);
			}
			else if (epoll_instance.events[i].events & EPOLLIN) {
				status_code = decide_action(react_fd);
				switch (status_code)
				{
				case FILE_TYPE:
					deal_with_file(react_fd);
					break;
				case MESSAGE_TYPE:
					tp.submit_to_pool(&receive_loop::deal_with_mesg,this,react_fd);
					break;
				case GPS_TYPE:
					tp.submit_to_pool(&receive_loop::deal_with_gps,this,react_fd);
					break;
				case GET_TYPE:
					tp.submit_to_pool(&receive_loop::deal_with_get_file, this, react_fd);
					break;
				default:
					LOG_INFO(
						"Closing:",
						ADDRSTR(connection_storage[react_fd].address),
						" Received unknown request: ",
						connection_storage[react_fd].buffer_for_pre_messsage);
					//LOG_CLOSE(connection_storage[react_fd].address);
					epoll_instance.remove_fd_from_epoll(react_fd);
					close_connection(react_fd);
					memset(
						connection_storage[react_fd].buffer_for_pre_messsage,
						0,
						BUFFER_SIZE);
					break;
				}
			}
		}
	}
	tp.shutdown_pool();
	for (auto& [addr, stream] : addr_to_stream) {
		if (stream != nullptr) {
			delete stream;
		}
	}
	LOG_INFO("Server quits.");
	exit(0);
}

int receive_loop::decide_action(int fd)
{
	ssize_t ret = 0;
	ret = read(fd, connection_storage[fd].buffer_for_pre_messsage, BUFFER_SIZE);
	if (ret < 0 && errno != EAGAIN) {
		LOG_ERROR_C(connection_storage[fd].address);
#ifdef DEBUG
		perror("Something happened while read from client");
#endif // DEBUG
		goto end;
	}
#ifdef DEBUG
	cout << "Read msg from client: " << connection_storage[fd].buffer_for_pre_messsage << endl;
#endif // DEBUG
	switch (connection_storage[fd].buffer_for_pre_messsage[0])
	{
	case 'f':return FILE_TYPE;
	case 'n':return GPS_TYPE;
	case 'g':return GET_TYPE;
	case 'm':return MESSAGE_TYPE;
	}
	end:return -1;
}

void receive_loop::deal_with_file(int fd)
{
#ifdef DEBUG
	auto id = std::this_thread::get_id();
	LOG_DEBUG("GPS is handled by thread: ", to_string(GETCURID(id)));
#endif // DEBUG
	char msg1 = '1';
	write(fd, &msg1, sizeof(msg1));
	char full_path[64]="./";
	char* msg = connection_storage[fd].buffer_for_pre_messsage;
	msg += 2;
	LOG_INFO("Receiving file from:", ADDRSTR(connection_storage[fd].address), ' ', msg);
	ssize_t size = atoi(strchr(msg, '/')+1);
	strncat(full_path, msg, strcspn(msg, "/"));
	int write_fd = open(full_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	connection_storage[fd].bytes_to_deal_with = size;
	connection_storage[fd].buf = new char[size];
	char* buf = connection_storage[fd].buf;
	ssize_t ret = 0;
	while (size)
	{
		ret=read(fd, buf, size);
		if (ret < 0)
		{
			LOG_ERROR_C(connection_storage[fd].address);
			perror("Receieve failed");
			exit(1);
		}
		size -= ret;
		buf += ret;
	}
	//di->fd = write_fd;
	buf = connection_storage[fd].buf;
	for (;;) {
		ret = write(write_fd,
			connection_storage[fd].buf,
			connection_storage[fd].bytes_to_deal_with);
		if (ret < 0) {
			LOG_ERROR_C(connection_storage[fd].address);
#ifdef DEBUG
			perror("Server write to local failed");
#endif // DEBUG
			exit(1);
		}
		connection_storage[fd].bytes_to_deal_with -= ret;
		connection_storage[fd].buf += ret;
		if (connection_storage[fd].bytes_to_deal_with <= 0) {
			break;
		}
	}
#ifdef DEBUG
	cout << "Success on receiving file: " << msg << endl;
#endif // DEBUG
	//LOG_FILE(connection_storage[fd].address, move(msg));
	delete buf;
	close(write_fd);
	memset(connection_storage[fd].buffer_for_pre_messsage, 0, BUFFER_SIZE);
}

void receive_loop::deal_with_mesg(int fd)
{
#ifdef DEBUG
	auto id = std::this_thread::get_id();
	LOG_DEBUG("GPS is handled by thread: ", to_string(GETCURID(id)));
#endif // DEBUG
	char* pt = connection_storage[fd].buffer_for_pre_messsage;
	char buffer[128]{ 0 };
	strcpy(buffer, pt+2);
	strcat(buffer, "\n");
	char code = '1';
	write(fd, &code, sizeof code);
	LOG_MSG(connection_storage[fd].address, move(buffer));
#ifdef DEBUG
	cout << "Success on receiving message: " << buffer;
#endif // DEBUG
	memset(connection_storage[fd].buffer_for_pre_messsage, 0, BUFFER_SIZE);
}

void receive_loop::deal_with_gps(int fd)
{
#ifdef DEBUG
	auto id = std::this_thread::get_id();
	LOG_DEBUG("GPS is handled by thread: ", to_string(GETCURID(id)));
#endif // DEBUG
	char file_name[32] = "gps_";
	in_addr tmp_addr = connection_storage[fd].address.sin_addr;
	strcat(file_name, inet_ntoa(tmp_addr));
	if (addr_to_stream[tmp_addr.s_addr] == nullptr) {
		addr_to_stream[tmp_addr.s_addr] = new ofstream(file_name, ios::app | ios::out);
		if (addr_to_stream[tmp_addr.s_addr]->fail()) {
			LOG_ERROR_C(connection_storage[fd].address);
			cout << "Open gps file failed\n";
			exit(1);
		}
	}
	char* pt = connection_storage[fd].buffer_for_pre_messsage;
	while (*pt == ' ') ++pt;
	char buffer[128]{ 0 };
	strcpy(buffer, pt+2);
	strcat(buffer, "\n");
	*addr_to_stream[tmp_addr.s_addr] << buffer;
	addr_to_stream[tmp_addr.s_addr]->flush();
	LOG_MSG(connection_storage[fd].address, "GPS content");
#ifdef DEBUG
	cout << "Success on receiving GPS: " << buffer;
#endif // DEBUG
	memset(connection_storage[fd].buffer_for_pre_messsage, 0, BUFFER_SIZE);
}

void receive_loop::deal_with_get_file(int fd)
{
#ifdef DEBUG
	auto id = std::this_thread::get_id();
	LOG_DEBUG("GPS is handled by thread: ", to_string(GETCURID(id)));
#endif // DEBUG
	//char* tmp_pt = &connection_storage[fd].buffer_for_pre_messsage[2];
	LOG_INFO("Receive file request from:",
		ADDRSTR(connection_storage[fd].address), ' ',
		&connection_storage[fd].buffer_for_pre_messsage[2]);
	string full_path("./");
	full_path += &connection_storage[fd].buffer_for_pre_messsage[2];
	//char full_path[BUFFER_SIZE+2] = "./";
	//strncat(full_path, tmp_pt, BUFFER_SIZE);
	struct stat st;
	stat(full_path.c_str(), &st);
	if (access(full_path.c_str(), R_OK) != 0 || !S_ISREG(st.st_mode)) {
		LOG_ERROR("No access to request file or it's not regular file.");
		//LOG_FILE(connection_storage[fd].address,
		//	"No access to request file or it's not regular file.");
		memset(connection_storage[fd].buffer_for_pre_messsage, 0, BUFFER_SIZE);
		char code = '0';
		write(fd, &code, sizeof code);
		return;
	}
	//char* ptr = full_path;

	int file_fd = open(full_path.c_str(), O_RDONLY);
	if (file_fd < 0) {
		perror("Open file failed");
		close(fd);
		memset(connection_storage[fd].buffer_for_pre_messsage, 0, BUFFER_SIZE);
		return;
	}
	fstat(file_fd, &st);
	string react_msg(&full_path.c_str()[2]);
	//ptr += 2;
	//strcat(ptr, "/");
	//strcat(ptr, to_string(st.st_size).data());
	react_msg += '/' + to_string(st.st_size);
	write(fd, react_msg.c_str(), react_msg.size());
	ssize_t ret = 0;
	char flag = '0';
	ret = recv(fd, &flag, sizeof(flag), 0);
	if (flag != '1' || ret <= 0) {
#ifdef DEBUG
		cout << "Receive flag from server failed.\n";
		cout << __LINE__ << endl;
#endif // DEBUG
		LOG_ERROR_C(connection_storage[fd].address);
		LOG_CLOSE(connection_storage[fd].address);
		epoll_instance.remove_fd_from_epoll(fd);
		close(fd);
		memset(connection_storage[fd].buffer_for_pre_messsage, 0, BUFFER_SIZE);
		return;
	}
	off_t off = 0;
	long send_size = st.st_size;
	while (send_size > 0) {
		ssize_t ret = sendfile(fd, file_fd, &off, send_size);
		if(ret < 0)
		{
			LOG_ERROR_C(connection_storage[fd].address);
		#ifdef DEBUG
			perror("Sendfile failed");
		#endif // DEBUG
			break;
		}
	#ifdef DEBUG
		cout << "have send :" << ret << endl;
	#endif // DEBUG
		send_size -= ret;
	}
	//LOG_FILE(connection_storage[fd].address, full_path);
	LOG_INFO("Success on receiving file from client:", ADDRSTR(connection_storage[fd].address));
	memset(connection_storage[fd].buffer_for_pre_messsage, 0, BUFFER_SIZE);
	close(file_fd);
}

void receive_loop::close_connection(int fd)
{
	in_addr tmp_addr = connection_storage[fd].address.sin_addr;
	if (addr_to_stream[tmp_addr.s_addr] != nullptr){
		if (addr_to_stream[tmp_addr.s_addr]->is_open()) {
			addr_to_stream[tmp_addr.s_addr]->close();
		}
		delete addr_to_stream[tmp_addr.s_addr];
		addr_to_stream[tmp_addr.s_addr] = nullptr;
	}
}

int receive_loop::get_prefix(int fd)
{
	for (auto& charc : connection_storage[fd].buffer_for_pre_messsage) {
		switch (charc)
		{
		case 'f':return FILE_TYPE;
		case 'g':return GPS_TYPE;
		default:charc = ' ';
		}
	}
	return MESSAGE_TYPE;
}

void receive_loop::alarm_handler(int)
{
	int save_errno = errno;
	char msg = '0';
	send(pipe_fd[1], &msg, 1, 0);
	errno = save_errno;
}

