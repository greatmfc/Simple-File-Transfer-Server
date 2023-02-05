#include <iostream>
#include <array>
#include <random>
#include <chrono>
#include <fstream>
#include "../all_module.h"
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
	std::mt19937_64 engine(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
	auto file = std::to_string(engine());
	auto path = "./" + file;
	auto content = file + std::to_string(engine()) + std::to_string(engine()) + std::to_string(engine());
	mfcslib::File instance(file, false, O_RDWR);
	instance.write(content);
	mfcslib::Socket remote(ip.c_str(), (uint16_t)port);
	send_msg_to(remote, "This is a test message.");
	std::cout << "Finishing message sending.\n";
	send_file_to(remote, instance);
	std::cout << "Finishing file sending.\n";
	get_file_from(remote, file);
	std::cout << "Finishing file receiving.\n";
	remove(path.c_str());
	std::cout << "Target server works properly. Removing temporary file.\n";
	return 0;
}
