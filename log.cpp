#if __cplusplus > 201703L
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
#include "common_headers.h"
#endif // __cplusplus > 201703L

log::log()
{
}

log::~log()
{
	//fclose(logfile_fd);
	//condition_var.notify_all();
	//log_file.flush();
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
	#ifdef DEBUG
		log_file.flush();
	#endif // DEBUG
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
	#ifdef DEBUG
		log_file.flush();
	#endif // DEBUG
		//container.emplace(content);
		//condition_var.notify_one();
	}
}

void log::init_log()
{
	if (keep_log) {
		time(&rawtime);
		time_info = localtime(&rawtime);
		strftime(log_name, 64, "./log_%Y-%m-%d", time_info);
		log_file.open(log_name, ios::app | ios::out);
		//logfile_fd = fopen(log_name,"a");
		if (!log_file.is_open()) {
			cerr << "Open log file failed\n";
		}
		//setvbuf(logfile_fd, NULL, _IONBF, 0);
		//thread log_thread(flush_all_missions);
		//log_thread.detach();
	}
}

/*
void* log::write_log()
{
	unique_lock<mutex> locker(mt);
	while (1)
	{
		condition_var.wait(locker);
		if (container.size() == 0) {
			return nullptr;
		}
		log_file << container.front().data();
		//fputs(container.front().data(), logfile_fd);
		container.pop();
	}
}
*/
