#include <iostream>
#include <array>
#include <random>
#include <chrono>
#include <fstream>
#include "../common_headers.h"
using std::cerr;
using std::array;
using std::stoi;
using std::ofstream;
constexpr auto usage_content = "Usage: ./test ip:port\n";
void parse_arg(const string& arg, string& ip, int& port) {
	auto index = arg.find_first_of(':');
	if (index == string::npos) {
		cerr << "Fail to locate port number.\n";
		exit(1);
	}
	ip = arg.substr(0,index);
	port = stoi(arg.substr(index+1));
}
auto main(int argc, char* argv[])->int {
	if (argc != 2) {
		cerr << usage_content;
		exit(1);
	}
	string ip;
	auto port = 0;
	parse_arg(argv[1], ip, port);
	setup st(ip.c_str(),port);
	std::mt19937_64 engine(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
	auto file = std::to_string(engine());
	auto path = "./" + file;
	ofstream output(path, std::ios::app | std::ios::out);
	output << file + std::to_string(engine()) + std::to_string(engine()) + std::to_string(engine());
	output.flush();
	output.close();
	send_msg sm(st, "This is a test message.");
	send_file sf(st, path);
	get_file gf(st, file);
	sm.write_to();
	std::cout << "Finishing message sending.\n";
	sf.write_to();
	std::cout << "Finishing file sending.\n";
	gf.get_it();
	std::cout << "Finishing file receiving.\n";
	remove(path.c_str());
	std::cout << "Target server work properly. Removing temporary file.\n";
	return 0;
}
