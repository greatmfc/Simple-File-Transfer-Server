#ifndef LOGGER_HPP
#define LOGGER_HPP
#include <ctime>
#include <string_view>
#include <arpa/inet.h>
#include <filesystem>
#include "../include/json.hpp"
#include "../include/io.hpp"
using std::ios;
using std::ofstream;
using std::string;
using std::filesystem::create_directory;

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
		log_file.write(content);
	#ifdef DEBUG
		std::cerr << content;
		std::cerr.flush();
	#endif // DEBUG
	}

	void init_log() {
		if (keep_log) {
			string path("./");
			try
			{
				mfcslib::File settings("./sft.json", false, mfcslib::RDONLY);
				mfcslib::json_parser js(settings);
				if (auto res = js.find("LogPath"); res) {
					if (auto val = std::get_if<string>(&res.value()._val); val != nullptr) {
						path = *val;
						if (path.back() != '/') path += '/';
						create_directory(path);
					}
				}
			}
			catch (const std::exception& e) {}
			timespec_get(&ts, TIME_UTC);
			auto currentTime = localtime(&ts.tv_sec);
			day = currentTime->tm_mday;
			char log_name[64] = { 0 };
			strftime(log_name, 64, (path + "log_%F").c_str(), currentTime);
			log_file.open(log_name, false, mfcslib::WRONLY);
			log_file.set_nonblocking();
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
	mfcslib::File log_file;
	bool keep_log = true;
};
#endif // !LOGGER_HPP
