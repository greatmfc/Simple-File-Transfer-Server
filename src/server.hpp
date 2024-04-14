#ifndef S_HPP
#define S_HPP
#include "../include/coroutine.hpp"
#include "../include/http.hpp"
#include "../include/io.hpp"
#include "epoll_utility.hpp"
#include "fields.h"
#include "logger.hpp"
#include <format>
#include <iostream>
#include <string_view>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <unistd.h>
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
#define ALARM_TIME 1800s
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
	HTTP_TYPE,
	GET_TYPE
};

struct data_info :public mfcslib::NetworkSocket
{
	data_info& operator=(mfcslib::NetworkSocket&& other) {
		mfcslib::NetworkSocket* pt_other = &other;
		int* pt_fd = reinterpret_cast<int*>(pt_other);
		sockaddr_in* pt_addr = reinterpret_cast<sockaddr_in*>(pt_fd + 1);
		this->_fd = *pt_fd;
		*pt_fd = -1;
		this->ip_port = *pt_addr;
		::memset(pt_addr, 0, sizeof(sockaddr_in));
		return *this;
	}
	string requests;
	co_handle task;
	bool is_write_awaiting = false;
	bool is_read_awaiting = false;
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
		DefaultPage,
		ListenPort
	};
	epoll_utility epoll_instance;
	unordered_map<int, data_info> connections;
	unordered_map<string, string> json_conf;
	static inline bool running;
	static inline int pipe_fd[2];

	int decide_action(int fd);
	co_handle handle_sft_file(int fd);
	void handle_sft_mesg(int fd);
	co_handle handle_sft_get_file(int fd);
	void close_connection(int fd);
	static void alarm_handler(int sig);
	co_handle handle_http(int fd);
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
	uint16_t port = DEFAULT_PORT;
	{
		try {
			mfcslib::File settings("./sft.json");
			settings.open_read_only();
			json_parser js(settings);
			json_conf[f_HttpPath] = "./";
			json_conf[f_FileReceived] = "./";
			json_conf[f_FileToSend] = "./";
			json_conf[f_DefaultPage] = "index.html";
			for (auto& [key, value] : js.get_obj()) {
				auto val = value.at<string>();
				if (key == f_ListenPort) {
					auto val = value.at<long long>();
					if (val) port = (uint16_t)*val;
					continue;
				}
				if (string_view str = *val; str != "")
					json_conf[key] = str;
				if (key != f_DefaultPage) {
					if (json_conf[key].back() != '/') json_conf[key] += '/';
					mkdir(json_conf[key].data(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IWOTH);
				}
			}
		}
		catch (std::exception& e) {}
	}
	mfcslib::ServerSocket localserver(port);
	running = true;
	int socket_fd = localserver.get_fd();
	localserver.set_nonblocking();
	epoll_instance.add_fd_or_event(socket_fd, false, true, 0);
	epoll_instance.add_fd_or_event(pipe_fd[0], false, false, 0);
	signal(SIGALRM, alarm_handler);
	alarm(ALARM_TIME.count());
	LOG_INFO("Server starts.");
	LOG_INFO("Listening on local: " + localserver.get_ip_port_s());
	mfcslib::timer<int> clock(ALARM_TIME);
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
					while (1) {
						auto res = localserver.accpet();
						if (!res.available()) break;
						LOG_ACCEPT(res.get_ip_port_s());
						auto accepted_fd = res.get_fd();
						epoll_instance.add_fd_or_event(accepted_fd, false, true, EPOLLOUT);
						epoll_instance.set_fd_no_block(accepted_fd);
						connections[accepted_fd] = std::move(res);
						clock.insert_or_update(accepted_fd);
					}
				} catch (const mfcslib::basic_exception& e) {
					LOG_ERROR("Accept failed: ", e.what());
				}
			}
			else if (epoll_instance.events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
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
				auto& di = connections[react_fd];
				co_handle& task = di.task;
				clock.insert_or_update(react_fd);
				if (di.is_read_awaiting) {
					task.resume();
				}
				else {
					switch (decide_action(react_fd))
					{
					case FILE_TYPE:
						task = handle_sft_file(react_fd);
						break;
					case MESSAGE_TYPE:
						handle_sft_mesg(react_fd);
						break;
					case GET_TYPE:
						task = handle_sft_get_file(react_fd);
						break;
					case HTTP_TYPE:
						task = handle_http(react_fd);
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
			}
			else if (epoll_instance.events[i].events & EPOLLOUT) {
				co_handle& task = connections[react_fd].task;
				clock.insert_or_update(react_fd);
				if (connections[react_fd].is_write_awaiting) {
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
	string& request = connections[fd].requests;
	auto ret = 0ll;
	do {
		char buffer[1024]{ 0 };
		ret = read(fd, buffer, 1023);
		request += buffer;
	} while (ret > 0);
	if (ret <= 0 && errno != EAGAIN) {
		LOG_ERROR_C(connections[fd].get_ip_port_s());
		return -1;
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
	case 'G': [[fallthrough]];
	case 'P':return HTTP_TYPE;
	}
	return -1;
}

co_handle receive_loop::handle_sft_file(int fd)
{
	char msg1 = '1';
	write(fd, &msg1, sizeof(msg1));
	data_info& current_mission = connections[fd];
	auto name_size = current_mission.requests.substr(2);
	current_mission.requests.clear();
	LOG_INFO("Receiving file from:", current_mission.get_ip_port_s(), ' ', name_size);
	auto idx = name_size.find('/');
	auto size = std::stoull(name_size.substr(idx + 1));
	string name = json_conf[f_FileReceived];
	name += name_size.substr(0, idx);
	mfcslib::File output_file(name);
	output_file.open(true, WRONLY);
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
					current_mission.is_read_awaiting = true;
					co_yield 1;
					continue;
				}
				ret += currentRet;
				bytesLeft = size - ret;
			#ifdef DEBUG
				mfcslib::progress_bar(ret, size);
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
						current_mission.is_read_awaiting = true;
						co_yield 1;
						continue;
					}
					ret += currentReturn;
					bytesWritten += currentReturn;
				#ifdef DEBUG
					mfcslib::progress_bar(bytesWritten, size);
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
		}
		else {
			LOG_ERROR("Not received complete file data.");
		}
	}
	catch (const mfcslib::basic_exception& e) {
		LOG_ERROR("Client:", current_mission.get_ip_port_s(),' ', e.what());
		LOG_ERROR("Not received complete file data.");
		LOG_CLOSE(current_mission.get_ip_port_s());
		close_connection(fd);
	}
	current_mission.is_read_awaiting = false;
	co_return;
}

void receive_loop::handle_sft_mesg(int fd)
{
	char code = '1';
	write(fd, &code, sizeof code);
	LOG_MSG(connections[fd].get_ip_port_s(), &connections[fd].requests[2]);
	connections[fd].requests.clear();
}

co_handle receive_loop::handle_sft_get_file(int fd)
{
	data_info& current_mission = connections[fd];
	string& request = current_mission.requests;
	LOG_INFO("Receive file request from:",
		current_mission.get_ip_port_s(), ' ',
		&request[2]);
	string full_path = json_conf[f_FileToSend];
	full_path += &request[2];
	if (full_path.back() == '\n') full_path.pop_back();
	request.clear();
	try {
		mfcslib::File requested_file(full_path); //throw runtime_error
		requested_file.open_read_only();
		string react_msg("/" + requested_file.size_string());
		write(fd, react_msg.c_str(), react_msg.size() + 1);
		ssize_t ret = 0;
		char flag = '0';
		while (1) {
			ret = recv(fd, &flag, sizeof(flag), 0);
			if (ret >= 0 || errno != EAGAIN) break;
			else {
				current_mission.is_read_awaiting = true;
				co_yield 1;
			}
		}
		if (flag != '1' || ret <= 0)
			throw peer_exception("Receive flag failed.");
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
			if (ret <= 0) {
				if (errno == EAGAIN) {
					current_mission.is_read_awaiting = false;
					current_mission.is_write_awaiting = true;
					co_yield 1;
					continue;
				}
				else {
					if (ret < 0) {
						LOG_ERROR_C(current_mission.get_ip_port_s());
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
			mfcslib::progress_bar((file_sz - send_size), file_sz);
		#endif // DEBUG
		}
	#ifdef DEBUG
		cout << "\nFinishing file sending." << endl;
	#endif // DEBUG
		LOG_INFO("Success on sending file to client:", current_mission.get_ip_s());
	}
	catch (const mfcslib::peer_exception& e) {
		LOG_ERROR("Client:", current_mission.get_ip_port_s(),' ', e.what());
		LOG_CLOSE(current_mission.get_ip_port_s());
		close_connection(fd);
	}
	catch (const mfcslib::file_exception& e) {
		LOG_ERROR("Client:", current_mission.get_ip_port_s(),' ', e.what());
		char code = '0';
		write(fd, &code, sizeof code);
	}
	current_mission.is_read_awaiting = false;
	current_mission.is_write_awaiting = false;
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

co_handle receive_loop::handle_http(int fd)
{
	data_info& current_mission = connections[fd];
	while (true) {
		response_header response;
		try {
			string target_http = json_conf[f_HttpPath];
			auto& request = current_mission.requests;
			if (request.empty()) {
				for (;;) {
					char buffer[1024]{ 0 };
					auto ret = current_mission.read(buffer, sizeof buffer - 1);
					if (ret == 0) {
						close_connection(fd);
						co_return;
					}
					else if (ret == -1) {
						break;
					}
					request += buffer;
				}
			}
			auto parse_result = parse_http_request(request);
			request.clear();
			auto& request_path = parse_result[hd_path];
			if (request_path != "") {
				if (request_path.back() == '?')
					request_path.pop_back();
				target_http += decode_url(request_path);
			}
			else
				target_http += json_conf[f_DefaultPage];
			LOG_INFO("Client ", current_mission.get_ip_port_s(), " requests HTTP for: ", target_http);
			loff_t off = 0;
			mfcslib::File send_page = target_http;
			send_page.open_read_only();
			auto file_size = send_page.size();
			auto sz = send_page.size() - off;
			if (auto ite = parse_result.find(hd_range); ite != parse_result.end()) {
				auto& raw_content = ite->second;
				auto start_idx = raw_content.find_first_of('=') + 1;
				auto split_idx = raw_content.find('-');
				auto start_bytes = std::stol(raw_content.substr(start_idx, split_idx - start_idx));
				auto end_bytes = file_size - 1;
				if (split_idx != raw_content.size() - 1) {
					end_bytes = std::stol(raw_content.substr(split_idx + 1, raw_content.size() - split_idx - 1));
				}
				off = start_bytes;
				sz = end_bytes - start_bytes + 1;
				response.add_status_code(206);
				response.add_server_info();
				response.add_date();
				response.add_Etag(send_page);
				response.add_last_modified(send_page);
				response.add_content_type(send_page.get_type());
				response.add_connection_type(false);
				response.add_content_range(start_bytes, end_bytes, file_size);
				LOG_INFO(std::format("Require {} for range from {} to {}.\n", send_page.filename(), start_bytes, end_bytes));
				response.add_content_length(sz);
				response.add_blank_line();
			}
			else {
				auto cetag = response.generate_Etag(send_page);
				auto n_ite = parse_result.find(hd_if_none_match);
				if (n_ite != parse_result.end() && n_ite->second == cetag) {
					response.add_status_code(304);
					response.add_date();
					response.add_Etag(cetag);
					response.add_last_modified(send_page);
					response.add_server_info();
					response.add_connection_type(false);
					response.add_blank_line();
					current_mission.write(response.data());
					goto next_round;
				}
				else {
					response.add_status_code(200);
					response.add_date();
					response.add_Etag(cetag);
					response.add_line("Accept-Ranges: bytes");
					response.add_last_modified(send_page);
					response.add_content_length(file_size);
					response.add_server_info();
					response.add_content_type(send_page.get_type());
					response.add_connection_type(false);
					response.add_blank_line();
				}
			}
			current_mission.write(response.data());
			ssize_t ret = 0;
			do {
				ret = sendfile64(current_mission.get_fd(), send_page.get_fd(), &off, sz);
				if (ret == -1) {
					if (errno != EAGAIN) {
						string err = "Error in sendfile: ";
						err += GETERR;
						LOG_ERROR("Client:", current_mission.get_ip_port_s(), " has error: ", err);
						close_connection(fd);
					}
					current_mission.is_write_awaiting = true;
					co_yield 1;
				}
			} while (off != (loff_t)file_size);
			LOG_INFO("Finish sending: " + send_page.filename());
			if (parse_result[hd_connection] == "close") {
				close_connection(fd);
				LOG_CLOSE(current_mission.get_ip_port_s());
				co_return;
			}
		}
		catch (const IO_exception& e) {
			LOG_ERROR("Client:", current_mission.get_ip_port_s(), " has error: ", e.what());
			response.add_status_code(404);
			response.add_line("Content-Length: 296");
			//response.add_content_length(sizeof not_found_html - 1);
			response.add_server_info();
			response.add_line("Content-Type: text/html; charset=utf-8");
			response.add_connection_type(false);
			response.add_blank_line();
			current_mission.write(response.data());
			current_mission.write(not_found_html);
		}
		catch (const std::exception& a) {
			LOG_ERROR("Client:", current_mission.get_ip_port_s(), " has error: ", a.what());
			response.add_status_code(403);
			response.add_line("Content-Length: 299");
			//response.add_content_length(sizeof forbidden_html - 1);
			response.add_server_info();
			response.add_line("Content-Type: text/html; charset=utf-8");
			response.add_connection_type(false);
			response.add_blank_line();
			current_mission.write(response.data());
			current_mission.write(forbidden_html);
		}
	next_round:
		current_mission.is_write_awaiting = false;
		current_mission.is_read_awaiting = true;
		co_yield 1;
	}
}
#endif // !S_HPP

