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
#include "../include/coroutine.hpp"
#include "../include/io.hpp"
#include "epoll_utility.hpp"
#include "logger.hpp"
#ifndef MAXARRSZ
#define MAXARRSZ 1024'000'000
#define NUMSTOP 20'000
#endif // !MAXARRSZ
#define LOG_INFO(...) if(log::get_instance()->enable_log()) log::get_instance()->process_and_submit(LINFO,__VA_ARGS__)
#define LOG_DEBUG(...) if(log::get_instance()->enable_log()) log::get_instance()->process_and_submit(LDEBUG,__VA_ARGS__)
#define LOG_VERBOSE if(log::get_instance()->enable_log()) log::get_instance()->process_and_submit(LDEBUG,"in ",__FILE__,':',std::to_string(__LINE__))
#define LOG_WARN(...) if(log::get_instance()->enable_log()) log::get_instance()->process_and_submit(LWARN,__VA_ARGS__)
#define LOG_ERROR(...) if(log::get_instance()->enable_log()) log::get_instance()->process_and_submit(LERROR,__VA_ARGS__)
#define LOG_MSG(_addr,_msg) LOG_INFO("Message from:",_addr,' ',_msg)
#define LOG_FILE(_addr,_msg) LOG_INFO("File request from:",_addr,' ',_msg)
#define LOG_ACCEPT(_addr) LOG_INFO("Accept from:",_addr)
#define LOG_CLOSE(_addr) LOG_INFO("Closing: ",_addr)
#define LOG_ERROR_C(_addr) LOG_ERROR("Client:",_addr,' ',strerror(errno))
#define GETERR strerror(errno)
#define DEFAULT_PORT 9007
#define ALARM_TIME 300s
#define TIMEOUT 30000
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
	HTTP_GET_TYPE,
	HTTP_POST_TPYE,
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
	co_handle task;
};

class receive_loop
{
public:
	receive_loop() = default;
	~receive_loop() = default;
	static void stop_loop(int sig);
	void loop();

private:
	enum
	{
		FileReceived,
		FileToSend,
		HTTPFiles,
		DefaultPage
	};
	epoll_utility epoll_instance;
	unordered_map<int, data_info> connections;
	unordered_map<int,string> json_conf;
	static inline bool running;
	static inline int pipe_fd[2];

	int decide_action(int fd);
	co_handle deal_with_file(int fd);
	void deal_with_mesg(int fd);
	co_handle deal_with_get_file(int fd);
	void close_connection(int fd);
	static void alarm_handler(int sig);
	co_handle deal_with_http(int fd, int type);
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
	epoll_instance.add_fd_or_event(pipe_fd[0], false, false, 0);
	{
		try {
			File settings("./sft.json", false, RDONLY);
			json_parser js(settings);
			for (auto& [key, value] : js.get_obj()) {
				auto val = value.at<string>();
				if (key == "HttpPath" && val != nullptr) {
					json_conf[HTTPFiles] = *val;
					if (json_conf[HTTPFiles].back() != '/') json_conf[HTTPFiles] += '/';
					create_directory(json_conf[HTTPFiles]);
				}
				else if (key == "FileReceived" && val != nullptr) {
					json_conf[FileReceived] = *val;
					if (json_conf[FileReceived].back() != '/') json_conf[FileReceived] += '/';
					create_directory(json_conf[FileReceived]);
				}
				else if (key == "FileToSend" && val != nullptr) {
					json_conf[FileToSend] = *val;
					if (json_conf[FileToSend].back() != '/') json_conf[FileToSend] += '/';
					create_directory(json_conf[FileToSend]);
				}
				else if (key == "DefaultPage" && val != nullptr) {
					json_conf[DefaultPage] = *val;
				}
			}
		}
		catch (std::exception& e) {}
	}
	signal(SIGALRM, alarm_handler);
	alarm(ALARM_TIME.count());
	LOG_INFO("Server starts.");
	mfcslib::timer<int> clock(ALARM_TIME);
#ifdef DEBUG
	cout << "Listening on local: " + localserver.get_ip_port_s() << endl;
#endif // DEBUG
	while (running) {
		int count = epoll_instance.wait_for_epoll(-1);
		if (count < 0) [[unlikely]] {
			if (errno != EINTR) [[unlikely]]
				LOG_ERROR("Error in epoll_wait: ", strerror(errno));
			continue;
		}
		for (int i = 0; i < count; ++i) {
			int react_fd = epoll_instance.events[i].data.fd;

			if (react_fd == socket_fd) {
				try {
					while (true) {
						auto res = localserver.accpet();
						if (!res.available()) break;
					#ifdef DEBUG
						cout << "Accept from client:" << res.get_ip_port_s() << endl;
					#endif // DEBUG
						LOG_ACCEPT(res.get_ip_port_s());
						auto accepted_fd = res.get_fd();
						epoll_instance.add_fd_or_event(accepted_fd, false, true, EPOLLOUT);
						epoll_instance.set_fd_no_block(accepted_fd);
						connections[accepted_fd] = std::move(res);
						clock.insert_or_update(accepted_fd);
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
				cout << "Disconnect from client:" << connections[react_fd].get_ip_port_s() << endl;
			#endif // DEBUG
				LOG_INFO("Disconnect from client: ",connections[react_fd].get_ip_port_s());
				close_connection(react_fd);
				clock.erase_value(react_fd);
				connections.erase(react_fd);
			}
			else if (react_fd == pipe_fd[0]) {
				int signal = 0;
				recv(pipe_fd[0], &signal, sizeof signal, 0);
				if (signal != SIGALRM) break;
				LOG_INFO("Tick.");
				alarm(ALARM_TIME.count());
				auto timeout_list = clock.clear_expired();
				for (const auto& i : timeout_list) {
					LOG_INFO("Timeout client: ", connections[i].get_ip_port_s());
					close_connection(i);
					connections.erase(i);
				}
			}
			else if (epoll_instance.events[i].events & EPOLLIN) {
				auto& di=connections[react_fd];
				co_handle& task = di.task;
				clock.insert_or_update(react_fd);
				int type = decide_action(react_fd);
				if (task.empty() || task.done()) {
					switch (type)
					{
					case FILE_TYPE:
						task = deal_with_file(react_fd);
						break;
					case MESSAGE_TYPE:
						deal_with_mesg(react_fd);
						break;
					case GET_TYPE:
						task = deal_with_get_file(react_fd);
						break;
					case HTTP_GET_TYPE: [[fallthrough]];
					case HTTP_POST_TPYE:
						task = deal_with_http(react_fd, type);
						break;
					default:
						LOG_INFO(
							"Closing:",
							di.get_ip_port_s(),
							" Received unknown request: ",
							di.requests);
						close_connection(react_fd);
						clock.erase_value(react_fd);
						connections.erase(react_fd);
						break;
					}
				}
				else task.resume();
			}
			else if (epoll_instance.events[i].events & EPOLLOUT) {
				co_handle& task = connections[react_fd].task;
				clock.insert_or_update(react_fd);
				if (!(task.empty() || task.done())) {
					task.resume();
				}
			}
		}
	}
	LOG_INFO("Server quits.");
	exit(0);
}

int receive_loop::decide_action(int fd)
{
	ssize_t ret = 0;
	char buffer[512]{ 0 };
	string& request = connections[fd].requests;
	while (true) {
		ret = read(fd, buffer, 511);
		if (ret <= 0) break;
		request += buffer;
		memset(buffer, 0, 511);
	}
	if (ret < 0 && errno != EAGAIN) {
		LOG_ERROR_C(connections[fd].get_ip_port_s());
#ifdef DEBUG
		perror("Something happened while read from client");
#endif // DEBUG
		goto end;
	}
#ifdef DEBUG
	//cout << "Read msg from client: " << request << endl;
#endif // DEBUG
	if (request.back() != '\n') {
		request.push_back('\n');
	}
	switch (request[0])
	{
	case 'f':return FILE_TYPE;
	case 'g':return GET_TYPE;
	case 'm':return MESSAGE_TYPE;
	case 'G':return HTTP_GET_TYPE;
	case 'P':return HTTP_POST_TPYE;
	}
	end:return -1;
}

co_handle receive_loop::deal_with_file(int fd)
{
	char msg1 = '1';
	write(fd, &msg1, sizeof(msg1));
	auto name_size = connections[fd].requests.substr(2);
	connections[fd].requests.clear();
	LOG_INFO("Receiving file from:", connections[fd].get_ip_port_s(), ' ', name_size);
	auto idx = name_size.find('/');
	auto size = std::stoull(name_size.substr(idx + 1));
	string name = "./";
	if (json_conf.contains(FileReceived))
		name = json_conf[FileReceived];
	name += name_size.substr(0, idx);
	mfcslib::File output_file(name, true, O_WRONLY);
	try {
		auto complete = false;
		if (size < MAXARRSZ) {
			auto bufferForFile = mfcslib::make_array<Byte>(size);
			auto ret = 0ll;
			auto bytesLeft = size;
			while (true) {
				auto currentRet = bufferForFile.read(fd, ret, bytesLeft);
			#ifdef DEBUG
				//cout << "Return from read:" << currentRet << endl;
			#endif // DEBUG
				if (currentRet < 0) {
					co_yield 1;
					continue;
				}
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
				while (ret < (MAXARRSZ - NUMSTOP)) {
					currentReturn = bufferForFile.read(fd, ret, MAXARRSZ - ret);
					if (currentReturn < 0) {
						co_yield 1;
						continue;
					}
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
		LOG_ERROR("Client:", connections[fd].get_ip_port_s(),' ', e.what());
		LOG_ERROR("Not received complete file data.");
		LOG_CLOSE(connections[fd].get_ip_port_s());
	#ifdef DEBUG
		cout << e.what() << endl;
		cout << "Not received complete file data in fd: " << fd << endl;
		cout << "Closing it...\n" << endl;
	#endif // DEBUG
		close_connection(fd);
	}
	co_return;
}

void receive_loop::deal_with_mesg(int fd)
{
	char code = '1';
	write(fd, &code, sizeof code);
	LOG_MSG(connections[fd].get_ip_port_s(), &connections[fd].requests[2]);
#ifdef DEBUG
	cout << "Success on receiving message: " << &connections[fd].requests[2];
#endif // DEBUG
	connections[fd].requests.clear();
}

co_handle receive_loop::deal_with_get_file(int fd)
{
	string& request = connections[fd].requests;
	LOG_INFO("Receive file request from:",
		connections[fd].get_ip_port_s(), ' ',
		&request[2]);
	string full_path("./");
	if(json_conf.contains(FileToSend))
		full_path = json_conf[FileToSend];
	full_path += &request[2];
	if (full_path.back() == '\n') full_path.pop_back();
	request.clear();
	try {
		File requested_file(full_path, false, O_RDONLY); //throw runtime_error
		string react_msg("/" + requested_file.size_string());
		write(fd, react_msg.c_str(), react_msg.size() + 1);
		ssize_t ret = 0;
		char flag = '0';
		while (1) {
			ret = recv(fd, &flag, sizeof(flag), 0);
			if (ret >= 0 || errno != EAGAIN) break;
			else co_yield 1;
		}
		if (flag != '1' || ret <= 0)
			throw std::domain_error("Receive flag failed.");
		off_t off = 0;
		uintmax_t send_size = requested_file.size();
		int file_fd = requested_file.get_fd();
	#ifdef DEBUG
		auto file_sz = requested_file.size();
	#endif // DEBUG
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
						LOG_ERROR_C(connections[fd].get_ip_port_s());
					#ifdef DEBUG
						perror("Sendfile failed");
					#endif // DEBUG
					}
					LOG_ERROR("Not received complete file data.");
					co_return;
				}
			}
			send_size -= ret;
		#ifdef DEBUG
			//cout << "Bytes left: " << send_size << endl;
			progress_bar((file_sz - send_size), file_sz);
		#endif // DEBUG
		}
	#ifdef DEBUG
		cout << "\nFinishing file sending." << endl;
	#endif // DEBUG
		LOG_INFO("Success on sending file to client:", connections[fd].get_ip_s());
	}
	catch (const std::domain_error& e) {
	#ifdef DEBUG
		cout << e.what() << endl;
	#endif // DEBUG
		LOG_ERROR("Client:", connections[fd].get_ip_port_s(),' ', e.what());
		LOG_CLOSE(connections[fd].get_ip_port_s());
		close_connection(fd);
	}
	catch (const std::runtime_error& e) {
	#ifdef DEBUG
		cout << e.what() << endl;
	#endif // DEBUG
		LOG_ERROR("Client:", connections[fd].get_ip_port_s(),' ', e.what());
		char code = '0';
		write(fd, &code, sizeof code);
	}
	co_return;
}

void receive_loop::close_connection(int fd)
{
	epoll_instance.remove_fd_from_epoll(fd);
}

void receive_loop::alarm_handler(int sig)
{
	int save_errno = errno;
	send(pipe_fd[1], &sig, 1, 0);
	errno = save_errno;
}

co_handle receive_loop::deal_with_http(int fd, int type)
{
	static char not_found_html[] = "<!DOCTYPE html>\n<html lang=\"en\">\n\n<head>\n\t<meta charset=\"UTF - 8\">\n\t<title>404</title>\n</head>\n\n<body>\n\t<div class=\"text\" style=\"text-align: center\">\n\t\t<h1> 404 Not Found </h1>\n\t\t<h1> Target file is not found on sft. </h1>\n\t</div>\n</body>\n\n</html>\n";
	string http_path = "./";
	if (json_conf.contains(HTTPFiles)) {
		http_path = json_conf[HTTPFiles];
	}
	try {
		while (true) {
			string target_http = http_path;
			auto& request = connections[fd].requests;
			auto idx = request.find_first_of('/');
			if (request[idx + 1] == ' ') {
				if (json_conf.contains(DefaultPage))
					target_http += json_conf[DefaultPage];
				else
					target_http += "index.html";
			}
			else {
				auto end_idx = request.find(' ', idx);
				target_http += request.substr(idx + 1, end_idx - idx - 1);
			}
			if (target_http.back() == '?')
				target_http.pop_back();
			if (type == HTTP_POST_TPYE) {
				string post_content = str_split(request, "\r\n").back();
			}
			request.clear();
			LOG_INFO("Client ", connections[fd].get_ip_port_s(), " requests HTTP for: ", target_http);
		#ifdef DEBUG
			cout << "Sending: " + target_http << endl;
		#endif // DEBUG
			File send_page(target_http, false, RDONLY);
			string response = "HTTP/1.1 200 OK\r\n";
			response += "Content-Length: " + send_page.size_string() + "\r\n";
			response += "Server: Simple-File-Transfer\r\n";
			response += "Connection: keep-alive\r\n\r\n";
			connections[fd].write(response);
			loff_t off = 0;
			auto sz = send_page.size();
			ssize_t ret = 0;
			do {
				ret = sendfile64(connections[fd].get_fd(), send_page.get_fd(), &off, sz);
				if (ret == -1 && errno != EAGAIN) {
					string err = "Error in sendfile: ";
					err += GETERR;
					throw std::domain_error(err);
				}
				else co_yield 1;
			} while (off != (loff_t)sz);
		#ifdef DEBUG
			cout << "Finish sending: " + send_page.filename() << endl;
		#endif // DEBUG
		}
	}
	catch (const std::runtime_error& e) {
	#ifdef DEBUG
		cout << e.what() << endl;
	#endif // DEBUG
		LOG_ERROR("Client:", connections[fd].get_ip_port_s(), " has error: ", e.what());
		string response = "HTTP/1.1 404 Not Found\r\n";
		response += "Content-Length: 248\r\n";
		response += "Server: Simple-File-Transfer\r\n";
		response += "Content-Type: text/html; charset=utf-8\r\n";
		response += "Connection: close\r\n\r\n";
		connections[fd].write(response);
		connections[fd].write(not_found_html);
	}
	catch (const std::domain_error& a) {
	#ifdef DEBUG
		cout << a.what() << endl;
	#endif // DEBUG
		LOG_ERROR("Client:", connections[fd].get_ip_port_s(), " has error: ", a.what());
		close_connection(fd);
	}
	co_return;
}
#endif // !S_HPP

