#include <ctime>
#include <cstring>
#include <iostream>
#include <string_view>
#include <fstream>
#include <arpa/inet.h>
#include "common_headers.h"
using std::cerr;
using std::ios;

log::log()
{
}

log::~log()
{
	log_file.close();
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

