#include <iostream>
#include <array>
#include <random>
#include <chrono>
#include <fstream>
#define val constexpr auto
#define var auto
#include "../common_headers.h"
using std::cerr;
using std::array;
using std::stoi;
using std::ofstream;
val usage_content = "Usage: ./test ip:port\n";
void parse_arg(const string& arg, string& port, int& ip) {
	//char* tmp = strchr(arg, ':');
	auto index = arg.find_first_of(':');
	if (index == string::npos) {
		cerr << "Fail to locate port number.\n";
		exit(1);
	}
	port = arg.substr(index + 1);
	ip = stoi(arg.substr(0, index));
}
auto main(int argc, char* argv[])->int {
	if (argc != 2) {
		cerr << usage_content;
		exit(1);
	}
	string ip;
	var port = 0;
	parse_arg(argv[1], ip, port);
	setup st(ip.c_str(),port);
	std::mt19937_64 engine(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
	var file = std::to_string(engine());
	var path = "./" + file;
	ofstream output(file,std::ios::app|std::ios::out);
	output << file;
	output.flush();
	output.close();
	send_msg sm(st, "This is a test message.");
	send_file sf(st, path);
	get_file gf(st, file);
	sm.write_to();
	sf.write_to();
	gf.get_it();
	remove(file.c_str());
	return 0;
}
