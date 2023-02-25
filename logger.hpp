#ifndef LOGGER_HPP
#define LOGGER_HPP
#include <ctime>
#include <cstring>
#include <iostream>
#include <string_view>
#include <fstream>
#include <arpa/inet.h>
using std::cerr;
using std::ios;
using std::ofstream;
using std::string;

enum log_enum
{
	LINFO,
	LDEBUG,
	LWARN,
	LERROR
};

class log
{
public:
	~log() {
		log_file.close();
	};

	template<typename ...Args>
	void process_and_submit(log_enum type, const Args& ...args) {
		if (keep_log) {
			timespec_get(&ts, TIME_UTC);
			char tmp[32]{ 0 };
			auto currentTime = localtime(&ts.tv_sec);
			if (day != currentTime->tm_mday) {
				log_file.close();
				init_log();
			}
			strftime(tmp, 32, "%F %T.", currentTime);
			string content(tmp);
			content += std::to_string(ts.tv_nsec).substr(0, 6);
			switch (type)
			{
			case LINFO:content += " [Info]:"; break;
			case LDEBUG:content += " [Debug]:"; break;
			case LWARN:content += " [Warn]:"; break;
			case LERROR:content += " [Error]:"; break;
			}
			content = (content + ... + args);
			if (content.back() != '\n') {
				content += '\n';
			}
			m_submit_missions(content);
		}
	}

	void init_log() {
		if (keep_log) {
			timespec_get(&ts, TIME_UTC);
			auto currentTime = localtime(&ts.tv_sec);
			day = currentTime->tm_mday;
			char log_name[64] = { 0 };
			strftime(log_name, 64, "./log_%F", currentTime);
			log_file.open(log_name, ios::app | ios::out);
			if (!log_file.is_open()) {
				cerr << "Open log file failed\n";
			}
		}
	}
	static inline log* get_instance() {
		static log log_object;
		return &log_object;
	}
	constexpr void no_logfile() {
		keep_log = false;
	};
	constexpr bool enable_log() {
		return keep_log;
	}

private:
	log() = default;
	timespec ts;
	int day = 0;
	ofstream log_file;
	bool keep_log = true;
	void m_submit_missions(const string& ct) {
		log_file << ct;
		log_file.flush();
	}
};
#endif // !LOGGER_HPP
