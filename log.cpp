#ifdef __cpp_modules
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

module;
#include <ctime>
#include <cstring>
#include <iostream>
#include <string_view>
#include <fstream>
#include <arpa/inet.h>
export module log;
import structs;

using std::string_view;
using std::ofstream;
using std::cerr;
using std::ios;

export{
	class log
	{
	public:
		~log();
		void submit_missions(string_view&& sv);
		void submit_missions(MyEnum&& type, const struct sockaddr_in& _addr, string_view&& msg);
		void init_log();
		static log* get_instance();
		//static void flush_all_missions() {
			//log::get_instance()->write_log();
		//}
		constexpr void no_logfile() {
			keep_log = false;
		};

	private:
		log();
		time_t rawtime;
		struct tm* time_info;
		ofstream log_file;
		//FILE* logfile_fd;
		bool keep_log = true;
		char log_name[64] = { 0 };
		//queue<string> container;
		//mutex mt;
		//condition_variable condition_var;

		//void* write_log();
	};
}
#else
#include <ctime>
#include <cstring>
#include <iostream>
#include <string_view>
#include <fstream>
#include <arpa/inet.h>
#include "common_headers.h"
using std::cerr;
using std::ios;
#endif // __cplusplus > 201703L

log::log()
{
}

log::~log()
{
	log_file.close();
	if (!keep_log) {
		remove(log_name);
	}
}

void log::submit_missions(string_view&& sv)
{
	if (keep_log) {
		char content[128] = { 0 };
		time(&rawtime);
		time_info = localtime(&rawtime);
		strftime(content, 64, "%Y-%m-%d %H:%M:%S ", time_info);
		strcat(content, "[Local]:");
		strcat(content, sv.data());
		if (strchr(content, '\n') == NULL) {
			strcat(content, "\n");
		}
		log_file << content;
		log_file.flush();
	}
}

void log::submit_missions(MyEnum&& type, const sockaddr_in& _addr, string_view&& msg)
{
	if (keep_log) {
		char content[128] = { 0 };
		time(&rawtime);
		time_info = localtime(&rawtime);
		strftime(content, 64, "%Y-%m-%d %H:%M:%S ", time_info);
		switch (type)
		{
		case FILE_TYPE: strcat(content, "[File]:"); break;
		case MESSAGE_TYPE: strcat(content, "[Message]:"); break;
		case ACCEPT: strcat(content, "[Accept]:"); break;
		case CLOSE: strcat(content, "[Closed]:"); break;
		case ERROR_TYPE: strcat(content, "[Error]:"); break;
		default: break;
		}
		strcat(content, inet_ntoa(_addr.sin_addr));
		strcat(content, " ");
		strcat(content, msg.data());
		if (strchr(content, '\n') == NULL) {
			strcat(content, "\n");
		}
		log_file << content;
		log_file.flush();
	}
}

void log::m_submit_missions(const string& ct)
{
	if (keep_log) {
		log_file << ct;
		log_file.flush();
	}
}

void log::init_log()
{
	if (keep_log) {
		time(&rawtime);
		time_info = localtime(&rawtime);
		strftime(log_name, 64, "./log_%Y-%m-%d", time_info);
		log_file.open(log_name, ios::app | ios::out);
		if (!log_file.is_open()) {
			cerr << "Open log file failed\n";
		}
	}
}

