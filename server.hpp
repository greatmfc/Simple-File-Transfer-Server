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
#include "include/coroutine.hpp"
#include "include/io.hpp"
#include "epoll_utility.hpp"
#include "logger.hpp"
#ifndef MAXARRSZ
#define MAXARRSZ 1024'000'000
#endif // !MAXARRSZ
#define LOG_INFO(...) log::get_instance()->process_and_submit(LINFO,__VA_ARGS__)
#define LOG_DEBUG(...) log::get_instance()->process_and_submit(LDEBUG,__VA_ARGS__)
#define LOG_VERBOSE log::get_instance()->process_and_submit(LDEBUG,"in ",__FILE__,':',std::to_string(__LINE__))
#define LOG_WARN(...) log::get_instance()->process_and_submit(LWARN,__VA_ARGS__)
#define LOG_ERROR(...) log::get_instance()->process_and_submit(LERROR,__VA_ARGS__)
#define LOG_MSG(_addr,_msg) LOG_INFO("Message from:",_addr,' ',_msg)
#define LOG_FILE(_addr,_msg) LOG_INFO("File request from:",_addr,' ',_msg)
#define LOG_ACCEPT(_addr) LOG_INFO("Accept from:",_addr)
#define LOG_CLOSE(_addr) LOG_INFO("Closing :",_addr)
#define LOG_ERROR_C(_addr) LOG_ERROR("Client:",_addr,' ',strerror(errno))
#define GETERR strerror(errno)
#define DEFAULT_PORT 9007
#define ALARM_TIME 1200
#define TIMEOUT 30000
#define WAIT 15
constexpr auto ori_val = TIMEOUT / WAIT;
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


struct data_info :public mfcslib::Socket
{
	data_info& operator=(mfcslib::Socket&& other) {
		mfcslib::Socket* pt_other = &other;
		int* pt_fd = reinterpret_cast<int*>(pt_other);
		sockaddr_in* pt_addr = reinterpret_cast<sockaddr_in*>(pt_fd + 1);
		this->_fd = *pt_fd;
		*pt_fd = -1;
		this->ip_port = *pt_addr;
		::memset(pt_addr, 0, sizeof(sockaddr_in));
		return *this;
	}
	void empty_buf() {
		requests.clear();
	}
	string requests;
};

class receive_loop
{
public:
	receive_loop() = default;
	~receive_loop() = default;
	static void stop_loop(int sig);
	void loop();

private:
	epoll_utility epoll_instance;
	unordered_map<int, data_info> connections;
	unordered_map<unsigned int, ofstream*> addr_to_stream;
	static inline bool running;
	static inline int pipe_fd[2];

	int decide_action(int fd);
	co_handle deal_with_file(int fd);
	void deal_with_mesg(int fd);
	[[deprecated("Might cause potential memory leaks.")]]
	void deal_with_gps(int fd);
	co_handle deal_with_get_file(int fd);
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
			if (count < 0) [[unlikely]] {
				if (errno != EINTR) [[likely]]
					LOG_ERROR("Error in epoll_wait: ", strerror(errno));
				continue;
			}
			goto co;
		}
		for (int i = 0; i < count; ++i) {
			int react_fd = epoll_instance.events[i].data.fd;

			if (react_fd == socket_fd) {
				try {
					while (true) {
						auto res = localserver.accpet();
						if (!res) break;
					#ifdef DEBUG
						cout << "Accept from client:" << res.value().get_ip_s() << endl;
					#endif // DEBUG
						LOG_ACCEPT(res.value().get_ip_s());
						auto accepted_fd = res.value().get_fd();
						epoll_instance.add_fd_or_event(accepted_fd, true, true, 0);
						epoll_instance.set_fd_no_block(accepted_fd);
						connections[accepted_fd] = std::move(res.value());
					}
				}
				catch (const std::exception& e) {
					LOG_ERROR("Accept failed: ", strerror(errno));
				#ifdef DEBUG
					perror("Accept failed");
				#endif // DEBUG
				}
			}
			else if (epoll_instance.events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
			#ifdef DEBUG
				cout << "Disconnect from client:" << connections[react_fd].get_ip_s() << endl;
			#endif // DEBUG
				LOG_INFO("Disconnect from client: ",connections[react_fd].get_ip_s());
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
					tasks.emplace_back(deal_with_file(react_fd));	
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
						connections[react_fd].get_ip_s(),
						" Received unknown request: ",
						connections[react_fd].requests);
					close_connection(react_fd);
					connections[react_fd].empty_buf();
					break;
				}
			}
		}
	co:
		if (!tasks.empty()) {
			wait_time = WAIT;
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
		ret = read(fd, buffer, 255);
		if (ret <= 0) break;
		connections[fd].requests += buffer;
		memset(buffer, 0, 256);
	}
	if (ret < 0 && errno != EAGAIN) {
		LOG_ERROR_C(connections[fd].get_ip_s());
#ifdef DEBUG
		perror("Something happened while read from client");
#endif // DEBUG
		goto end;
	}
#ifdef DEBUG
	cout << "Read msg from client: " << connections[fd].requests << endl;
#endif // DEBUG
	if (connections[fd].requests.back() != '\n') {
		connections[fd].requests.push_back('\n');
	}
	switch (connections[fd].requests[0])
	{
	case 'f':return FILE_TYPE;
	case 'n':return GPS_TYPE;
	case 'g':return GET_TYPE;
	case 'm':return MESSAGE_TYPE;
	}
	end:return -1;
}

co_handle receive_loop::deal_with_file(int fd)
{
	char msg1 = '1';
	write(fd, &msg1, sizeof(msg1));
	auto name_size = connections[fd].requests.substr(2);
	LOG_INFO("Receiving file from:", connections[fd].get_ip_port_s(), ' ', name_size);
	auto idx = name_size.find('/');
	auto size = std::stoull(name_size.substr(idx + 1));
	auto name = "./" + name_size.substr(0, idx);
	mfcslib::File output_file(name, true, O_WRONLY);
	try {
		auto complete = false;
		int max_times = ori_val;
		if (size < MAXARRSZ) {
			auto bufferForFile = mfcslib::make_array<Byte>(size);
			auto ret = 0ll;
			auto bytesLeft = size;
			while (true) {
				auto currentRet = bufferForFile.read(fd, ret, bytesLeft);
			#ifdef DEBUG
				cout << "Return from read:" << currentRet << endl;
			#endif // DEBUG
				if (currentRet < 0) {
					if (max_times-- <= 0) {
					#ifdef DEBUG
						cout << "Max times is:" << max_times << endl;
					#endif // DEBUG
						throw std::runtime_error("Connection timeout.");
					}
					co_yield 1;
					continue;
				}
				max_times = ori_val;
				ret += currentRet;
				bytesLeft = size - ret;
			#ifdef DEBUG
				//progress_bar(ret, size);
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
					if (currentReturn < 0) {
						if (max_times-- <= 0) {
						#ifdef DEBUG
							cout << "Max times is:" << max_times << endl;
						#endif // DEBUG
							throw std::runtime_error("Connection timeout.");
						}
						co_yield 1;
						continue;
					}
					max_times = ori_val;
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
			cout << "\nSuccess on receiving file: " << name_size << endl;
	#endif // DEBUG
		}
		else {
			LOG_ERROR("Not received complete file data.");
	#ifdef DEBUG
			cout << "\nNot received complete file data." << endl;
	#endif // DEBUG
		}
	}
	catch (const std::exception& e) {
		LOG_ERROR("Client:", connections[fd].get_ip_s(),' ', e.what());
		LOG_CLOSE(connections[fd].get_ip_s());
	#ifdef DEBUG
		cout << e.what() << endl;
	#endif // DEBUG
		close_connection(fd);
		goto end;
	}
	epoll_instance.add_fd_or_event(fd, true, true, 0);
end:
	connections[fd].requests.clear();
	co_return;
}

void receive_loop::deal_with_mesg(int fd)
{
	char code = '1';
	write(fd, &code, sizeof code);
	LOG_MSG(connections[fd].get_ip_s(), &connections[fd].requests[2]);
#ifdef DEBUG
	cout << "Success on receiving message: " << &connections[fd].requests[2];
#endif // DEBUG
	connections[fd].requests.clear();
}

void receive_loop::deal_with_gps(int fd)
{
	char file_name[32] = "gps_";
	in_addr tmp_addr = connections[fd].get_ip();
	strcat(file_name, inet_ntoa(tmp_addr));
	if (addr_to_stream[tmp_addr.s_addr] == nullptr) {
		addr_to_stream[tmp_addr.s_addr] = new ofstream(file_name, ios::app | ios::out);	
		if (addr_to_stream[tmp_addr.s_addr]->fail()) [[unlikely]] {
			LOG_ERROR("Error while creating gps file.");
			LOG_CLOSE(connections[fd].get_ip_s());
			close_connection(fd);
			connections[fd].requests.clear();
			return;
		}
	}
	*addr_to_stream[tmp_addr.s_addr] << connections[fd].requests.substr(2);
	addr_to_stream[tmp_addr.s_addr]->flush();
	LOG_MSG(connections[fd].get_ip_s(), "GPS content");
#ifdef DEBUG
	cout << "Success on receiving GPS: " << &connections[fd].requests[2];
	cout.flush();
#endif // DEBUG
	connections[fd].requests.clear();
	epoll_instance.add_fd_or_event(fd, true, true, 0);
}

mfcslib::co_handle receive_loop::deal_with_get_file(int fd)
{
	LOG_INFO("Receive file request from:",
		connections[fd].get_ip_s(), ' ',
		&connections[fd].requests[2]);
	string full_path("./");
	full_path += &connections[fd].requests[2];
	if (full_path.back() == '\n') full_path.pop_back();
	struct stat st;
	stat(full_path.c_str(), &st);
	if (access(full_path.c_str(), R_OK) != 0 || !S_ISREG(st.st_mode)) {
		LOG_ERROR("No access to request file or it's not regular file.");
		connections[fd].requests.clear();
		char code = '0';
		write(fd, &code, sizeof code);
		co_return;
	}

	int file_fd = open(full_path.c_str(), O_RDONLY);
	if (file_fd < 0) {
		LOG_ERROR("Open requested file fail: ", strerror(errno));
		char code = '0';
		write(fd, &code, sizeof code);
		connections[fd].requests.clear();
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
		LOG_ERROR_C(connections[fd].get_ip_s());
		LOG_CLOSE(connections[fd].get_ip_s());
		close_connection(fd);
		connections[fd].requests.clear();
		co_return;
	}
	off_t off = 0;
	uintmax_t send_size = st.st_size;
	while (send_size > 0) {
		ssize_t ret = sendfile(fd, file_fd, &off, send_size);
	#ifdef DEBUG
		cout << "Return from sendfile: " << ret << endl;
	#endif // DEBUG
		if (ret <= 0)
		{
			if (errno == EAGAIN) {
				co_yield 1;
				continue;
			}
			else {
				if (ret < 0) {
					LOG_ERROR_C(connections[fd].get_ip_s());
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
		//progress_bar((st.st_size - send_size), st.st_size);
	#endif // DEBUG
	}
#ifdef DEBUG
	cout << "\nFinishing file sending." << endl;
#endif // DEBUG
	LOG_INFO("Success on sending file to client:", connections[fd].get_ip_s());
end:
	connections[fd].requests.clear();
	epoll_instance.add_fd_or_event(fd, true, true, 0);
	co_return;
}

void receive_loop::close_connection(int fd)
{
	in_addr tmp_addr = connections[fd].get_ip();
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

