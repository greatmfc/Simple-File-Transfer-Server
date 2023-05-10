#ifndef CL_HPP
#define CL_HPP
#include <cstring>
#include <cassert>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <exception>
#include <iostream>
#include "../include/io.hpp"
#define BUFFER_SIZE 64
#define MAXARRSZ 1024'000'000
#define NUMSTOP 20'000
using std::cout;
using std::cerr;
using std::endl;
using std::to_string;
using std::invalid_argument;
using std::string_view;
using namespace mfcslib;

void send_msg_to(mfcslib::Socket& target, const string_view& msg) {
	string request("m/");
	request += msg;
	target.write(request);
	char code = target.read();
	if (code != '1' && errno != 0) {
		perror("Something wrong with the server");
	}
}
void send_file_to(mfcslib::Socket& target, mfcslib::File& file) {
	string request = "f/" + file.filename() + '/' + file.size_string();
	target.write(request);
	char code = target.read();
	if (code != '1') {
		string error_msg = "Error while receving code from server in send file:\n";
		error_msg += strerror(errno);
		throw runtime_error(error_msg);
	}
	off_t off = 0;
	uintmax_t have_send = 0;
	auto file_sz = file.size();
	while (have_send < file_sz) {
		auto ret = sendfile(target.get_fd(), file.get_fd(), &off, file_sz);
		if(ret < 0)
		{
			perror("Sendfile failed");
			break;
		}
		have_send += ret;
	#ifdef DEBUG
		cout << "have send :" << ret << endl;
	#endif // DEBUG
		progress_bar(have_send, file_sz);
	}
	cout << '\n';
}
void get_file_from(mfcslib::Socket& tartget, const string& file) {
	string request = "g/";
	auto idx = file.find('/');
	if (idx != string::npos) {
		request += file.substr(idx + 1);
	}
	else request += file;
	tartget.write(request);
	TypeArray<Byte> msg(BUFFER_SIZE);
	auto ret = tartget.read(msg);
	if (ret <= 1) {
		cerr<<"File might not be found in the server.\n";
		exit(1);
	}
	tartget.write("1");

	mfcslib::File file_output_stream(file, true, O_WRONLY);
	auto msg_string = msg.to_string();
	auto sizeOfFile = std::stoull(msg_string.substr(msg_string.find_last_of('/')+1));
	if (sizeOfFile < MAXARRSZ) {
		auto bufferForFile = mfcslib::make_array<Byte>(sizeOfFile);
		ssize_t ret = 0;
		auto bytesLeft = sizeOfFile;
		while (true) {
			ret += tartget.read(bufferForFile, ret, bytesLeft);
			bytesLeft = sizeOfFile - ret;
			progress_bar(ret, sizeOfFile);
			if (bytesLeft <= 0) break;
		}
		file_output_stream.write(bufferForFile);
	}
	else {
		auto bufferForFile = mfcslib::make_array<Byte>(MAXARRSZ);
		auto ret = 0ull;
		auto bytesWritten = ret;
		while (true) {
			ssize_t currentReturn = 0;
			while (ret < (MAXARRSZ - NUMSTOP)) {
				currentReturn = tartget.read(bufferForFile, ret, MAXARRSZ - ret);
				if (currentReturn <= 0) break;
				ret += currentReturn;
				if (ret + bytesWritten >= sizeOfFile) break;
			}
			if (currentReturn <= 0) break;
			file_output_stream.write(bufferForFile, 0, ret);
			bytesWritten += ret;
			progress_bar(bytesWritten, sizeOfFile);
			if (bytesWritten >= sizeOfFile) break;
			bufferForFile.empty_array();
			ret = 0;
		}
	}
	cout << '\n';
}
#endif
