#ifndef S_HPP
#define S_HPP
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
#include <list>
#include <unordered_map>
#include "coroutine.hpp"
#include "io.hpp"
#include "epoll_utility.hpp"
#include "logger.hpp"
#ifndef MAXARRSZ
#define MAXARRSZ 1024'000'000
#endif // !MAXARRSZ
#define ADDRSTR(_addr) inet_ntoa(_addr.sin_addr)
#define LOG_INFO(...) log::get_instance()->process_and_submit(LINFO,__VA_ARGS__)
#define LOG_DEBUG(...) log::get_instance()->process_and_submit(LDEBUG,__VA_ARGS__)
#define LOG_VERBOSE log::get_instance()->process_and_submit(LDEBUG,"in ",__FILE__,':',std::to_string(__LINE__))
#define LOG_WARN(...) log::get_instance()->process_and_submit(LWARN,__VA_ARGS__)
#define LOG_ERROR(...) log::get_instance()->process_and_submit(LERROR,__VA_ARGS__)
#define LOG_MSG(_addr,_msg) LOG_INFO("Message from:",ADDRSTR(_addr),' ',_msg)
#define LOG_FILE(_addr,_msg) LOG_INFO("File request from:",ADDRSTR(_addr),' ',_msg)
#define LOG_ACCEPT(_addr) LOG_INFO("Accept from:",ADDRSTR(_addr))
#define LOG_CLOSE(_addr) LOG_INFO("Closing :",ADDRSTR(_addr))
#define LOG_ERROR_C(_addr) LOG_ERROR("Client:",ADDRSTR(_addr),' ',strerror(errno))
#define GETERR strerror(errno)
#define DEFAULT_PORT 9007
#define ALARM_TIME 600
using std::cout;
using std::endl;
using std::to_string;
using std::ios;
using std::unordered_map;
using std::string;
using std::ofstream;
using namespace mfcslib;

enum MyEnum
{
	FILE_TYPE,
	MESSAGE_TYPE,
	NONE_TYPE,
	ERROR_TYPE,
	GPS_TYPE,
	ACCEPT,
	CLOSE,
	READ_PRE,
	READ_MSG,
	READ_DATA,
	WRITE_DATA,
	GET_TYPE
};


typedef struct data_info
{
	int reserved_var[8];
	ssize_t bytes_to_deal_with;
	char* buf;
	string buffer_for_pre_messsage;
	struct iovec iov;
	sockaddr_in address;
} data_info;

class receive_loop
{
public:
	receive_loop() = default;
	~receive_loop() = default;
	static void stop_loop(int sig);
	void loop();

private:
	epoll_utility epoll_instance;
	unordered_map<int, data_info> connection_storage;
	unordered_map<unsigned int, ofstream*> addr_to_stream;
	static inline bool running;
	static inline int pipe_fd[2];

	int decide_action(int fd);
	void deal_with_file(int fd);
	void deal_with_mesg(int fd);
	void deal_with_gps(int fd);
	mfcslib::co_handle deal_with_get_file(int fd);
	void close_connection(int fd);
	static void alarm_handler(int sig);
};

void receive_loop::stop_loop(int sig)
{
	running = false;
	send(pipe_fd[1], &sig, 1, 0);
	LOG_INFO("Server quits.");
	exit(0);
}

void receive_loop::loop()
{
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipe_fd) < 0) {
		string error_msg("Error in creating socket pair: ");
		error_msg += GETERR;
		throw runtime_error(error_msg);
	}
	mfcslib::ServerSocket localserver(DEFAULT_PORT);
	running = true;
	sockaddr_in addr{};
	socklen_t len = sizeof addr;
	int socket_fd = localserver.get_fd();
	localserver.set_nonblocking();
	epoll_instance.add_fd_or_event(socket_fd, false, true, 0);
	epoll_instance.add_fd_or_event(pipe_fd[0], false, true, 0);
	signal(SIGALRM, alarm_handler);
	alarm(ALARM_TIME);
	LOG_INFO("Server starts.");
	int wait_time = -1;
	std::list<mfcslib::co_handle> tasks;
	//thread_pool tp;
	//tp.init_pool();
	while (running) {
		int count = epoll_instance.wait_for_epoll(wait_time);
		if (count <= 0) {
			if (count < 0)	LOG_ERROR("Error in epoll_wait: ", strerror(errno));
			goto co;
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
					epoll_instance.add_fd_or_event(accepted_fd, true, true, 0);
					epoll_instance.set_fd_no_block(accepted_fd);
					connection_storage[accepted_fd].address = addr;
				}
			}
			else if (epoll_instance.events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
			#ifdef DEBUG
				cout << "Disconnect from client:" << inet_ntoa(connection_storage[react_fd].address.sin_addr) << endl;
			#endif // DEBUG
				LOG_INFO("Disconnect from client: ",ADDRSTR(connection_storage[react_fd].address));
				close_connection(react_fd);
			}
			else if (react_fd == pipe_fd[0]) {
				int signal = 0;
				recv(pipe_fd[0], &signal, sizeof signal, 0);
				if (signal != SIGALRM) break;
				LOG_INFO("Tick.");
				alarm(ALARM_TIME);
			}
			else if (epoll_instance.events[i].events & EPOLLIN) {
				auto status_code = decide_action(react_fd);
				switch (status_code)
				{
				case FILE_TYPE:
					deal_with_file(react_fd);
					break;
				case MESSAGE_TYPE:
					//tp.submit_to_pool(&receive_loop::deal_with_mesg,this,react_fd);
					deal_with_mesg(react_fd);
					break;
				case GPS_TYPE:
					//tp.submit_to_pool(&receive_loop::deal_with_gps,this,react_fd);
					deal_with_gps(react_fd);
					break;
				case GET_TYPE:
					//tp.submit_to_pool(&receive_loop::deal_with_get_file, this, react_fd);
					tasks.emplace_back(deal_with_get_file(react_fd));
					break;
				default:
					LOG_INFO(
						"Closing:",
						ADDRSTR(connection_storage[react_fd].address),
						" Received unknown request: ",
						connection_storage[react_fd].buffer_for_pre_messsage);
					close_connection(react_fd);
					connection_storage[react_fd].buffer_for_pre_messsage.clear();
					break;
				}
			}
		}
	co:
		if (!tasks.empty()) {
			wait_time = 10;
			for (ssize_t i = tasks.size(); i-- > 0;) {
				for (auto ite = tasks.begin(); ite != tasks.end(); ++ite) {
					if (ite->done()) {
						tasks.erase(ite);
						break;
					}
					else ite->resume();
				}
			}
		}
		else {
			wait_time = -1;
		}
	}
	//tp.shutdown_pool();
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
	char buffer[256]{ 0 };
	while (true) {
		ret = read(fd, buffer, 256);
		if (ret <= 0) break;
		connection_storage[fd].buffer_for_pre_messsage += buffer;
		memset(buffer, 0, 256);
	}
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
	if (connection_storage[fd].buffer_for_pre_messsage.back() != '\n') {
		connection_storage[fd].buffer_for_pre_messsage.push_back('\n');
	}
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
	char msg1 = '1';
	write(fd, &msg1, sizeof(msg1));
	//char full_path[64]="./";
	auto name_size = connection_storage[fd].buffer_for_pre_messsage.substr(2);
	//msg += 2;
	LOG_INFO("Receiving file from:", ADDRSTR(connection_storage[fd].address), ' ', name_size);
	auto idx = name_size.find('/');
	auto size = std::stoull(name_size.substr(idx + 1));
	auto name = "./" + name_size.substr(0, idx);
	mfcslib::File output_file(name, true, O_WRONLY);
	try {
		auto complete = false;
		if (size < MAXARRSZ) {
			auto bufferForFile = mfcslib::make_array<Byte>(size);
			auto ret = 0ll;
			auto bytesLeft = size;
			while (true) {
				auto currentRet = bufferForFile.read(fd, ret, bytesLeft);
				if (currentRet < 0) continue;
				ret += currentRet;
				bytesLeft = size - ret;
			#ifdef DEBUG
				progress_bar(ret, size);
			#endif // DEBUG
				if (bytesLeft <= 0 || currentRet == 0) break;
			}
			if (bytesLeft <= 0) complete = true;
			output_file.write(bufferForFile);

		}
		else {
			auto bufferForFile = mfcslib::make_array<Byte>(MAXARRSZ);
			auto ret = 0ll;
			size_t bytesWritten = ret;
			while (true) {
				auto currentReturn = 0ll;
				while (ret < (MAXARRSZ - 200'000)) {
					currentReturn = bufferForFile.read(fd, ret, MAXARRSZ - ret);
					if (currentReturn < 0) continue;
					ret += currentReturn;
					bytesWritten += currentReturn;
				#ifdef DEBUG
					progress_bar(bytesWritten, size);
				#endif // DEBUG
					if (bytesWritten >= size || currentReturn == 0) break;
				}
				output_file.write(bufferForFile, 0, ret);
				if (bytesWritten >= size) {
					complete = true;
					break;
				}
				if (currentReturn == 0) break;
				bufferForFile.empty_array();
				ret = 0;
			}
		}
		if (complete) {
			LOG_INFO("Success on receiving file: ", name_size);
	#ifdef DEBUG
			cout << "\nSuccess on receiving file: " << name_size;
	#endif // DEBUG
		}
		else {
			LOG_ERROR("Not received complete file data.");
	#ifdef DEBUG
			cout << "\nNot received complete file data.\n";
	#endif // DEBUG
		}
	#ifdef DEBUG
		cout.flush();
	#endif // DEBUG
	}
	catch (const std::exception& e) {
		LOG_ERROR("Client:", ADDRSTR(connection_storage[fd].address), e.what());
		LOG_CLOSE(connection_storage[fd].address);
	#ifdef DEBUG
		cout << e.what() << '\n';
	#endif // DEBUG
	}
	connection_storage[fd].buffer_for_pre_messsage.clear();
}

void receive_loop::deal_with_mesg(int fd)
{
	char code = '1';
	write(fd, &code, sizeof code);
	LOG_MSG(connection_storage[fd].address, &connection_storage[fd].buffer_for_pre_messsage[2]);
#ifdef DEBUG
	cout << "Success on receiving message: " << &connection_storage[fd].buffer_for_pre_messsage[2];
#endif // DEBUG
	connection_storage[fd].buffer_for_pre_messsage.clear();
}

void receive_loop::deal_with_gps(int fd)
{
	char file_name[32] = "gps_";
	in_addr tmp_addr = connection_storage[fd].address.sin_addr;
	strcat(file_name, inet_ntoa(tmp_addr));
	if (addr_to_stream[tmp_addr.s_addr] == nullptr) {
		addr_to_stream[tmp_addr.s_addr] = new ofstream(file_name, ios::app | ios::out);
		if (addr_to_stream[tmp_addr.s_addr]->fail()) {
			LOG_ERROR("Error while creating gps file.");
			LOG_CLOSE(connection_storage[fd].address);
			close_connection(fd);
			return;
		}
	}
	*addr_to_stream[tmp_addr.s_addr] << connection_storage[fd].buffer_for_pre_messsage.substr(2);
	addr_to_stream[tmp_addr.s_addr]->flush();
	LOG_MSG(connection_storage[fd].address, "GPS content");
#ifdef DEBUG
	cout << "Success on receiving GPS: " << &connection_storage[fd].buffer_for_pre_messsage[2];
	cout.flush();
#endif // DEBUG
	connection_storage[fd].buffer_for_pre_messsage.clear();
}

mfcslib::co_handle receive_loop::deal_with_get_file(int fd)
{
	LOG_INFO("Receive file request from:",
		ADDRSTR(connection_storage[fd].address), ' ',
		&connection_storage[fd].buffer_for_pre_messsage[2]);
	string full_path("./");
	full_path += &connection_storage[fd].buffer_for_pre_messsage[2];
	if (full_path.back() == '\n') full_path.pop_back();
	struct stat st;
	stat(full_path.c_str(), &st);
	if (access(full_path.c_str(), R_OK) != 0 || !S_ISREG(st.st_mode)) {
		LOG_ERROR("No access to request file or it's not regular file.");
		connection_storage[fd].buffer_for_pre_messsage.clear();
		char code = '0';
		write(fd, &code, sizeof code);
		co_return;
	}

	int file_fd = open(full_path.c_str(), O_RDONLY);
	if (file_fd < 0) {
		LOG_ERROR("Open requested file fail: ", strerror(errno));
		char code = '0';
		write(fd, &code, sizeof code);
		connection_storage[fd].buffer_for_pre_messsage.clear();
		co_return;
	}
	fstat(file_fd, &st);
	string react_msg(&full_path.c_str()[2]);
	react_msg += '/' + to_string(st.st_size);
	write(fd, react_msg.c_str(), react_msg.size());
	ssize_t ret = 0;
	char flag = '0';
	while (1) {
		ret = recv(fd, &flag, sizeof(flag), 0);
		if (ret >= 0 || errno != EAGAIN) break;
		else co_yield 1;
	}
	if (flag != '1' || ret <= 0) {
#ifdef DEBUG
		cout << "Receive flag from server failed.\nIn line: ";
		cout << __LINE__ << endl;
#endif // DEBUG
		LOG_ERROR_C(connection_storage[fd].address);
		LOG_CLOSE(connection_storage[fd].address);
		close_connection(fd);
		connection_storage[fd].buffer_for_pre_messsage.clear();
		co_return;
	}
	off_t off = 0;
	uintmax_t send_size = st.st_size;
	while (send_size > 0) {
		ssize_t ret = sendfile(fd, file_fd, &off, send_size);
	#ifdef DEBUG
		//cout << "Return from sendfile: " << ret << endl;
	#endif // DEBUG
		if (ret <= 0)
		{
			if (errno == EAGAIN) {
				co_yield 1;
				continue;
			}
			else {
				if (ret < 0) {
					LOG_ERROR_C(connection_storage[fd].address);
					//LOG_CLOSE(connection_storage[fd].address);
					//close_connection(fd);
				#ifdef DEBUG
					perror("Sendfile failed");
				#endif // DEBUG
				}
				LOG_ERROR("Not received complete file data.");
				goto end;
			}
			//break;
		}
		send_size -= ret;
	#ifdef DEBUG
		progress_bar((st.st_size - send_size), st.st_size);
	#endif // DEBUG
	}
#ifdef DEBUG
	cout << "\nFinishing file sending." << endl;
#endif // DEBUG
	LOG_INFO("Success on sending file to client:", ADDRSTR(connection_storage[fd].address));
end:connection_storage[fd].buffer_for_pre_messsage.clear();
	epoll_instance.add_fd_or_event(fd, true, true, 0);
	co_return;
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
	epoll_instance.remove_fd_from_epoll(fd);
}

void receive_loop::alarm_handler(int sig)
{
	int save_errno = errno;
	send(pipe_fd[1], &sig, 1, 0);
	errno = save_errno;
}
#endif // !S_HPP

