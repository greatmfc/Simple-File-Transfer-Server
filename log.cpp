#ifdef ENABLE_MODULES
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
		timespec_get(&ts, TIME_UTC);
		auto currentTime = localtime(&ts.tv_sec);
		day = currentTime->tm_mday;
		strftime(log_name, 64, "./log_%F", currentTime);
		log_file.open(log_name, ios::app | ios::out);
		if (!log_file.is_open()) {
			cerr << "Open log file failed\n";
		}
	}
}

