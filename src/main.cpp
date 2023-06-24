#include <csignal>
#include <iostream>
#include <getopt.h>
#include <cstring>
#include <cstdio>
#include <sys/stat.h>
#include <locale>
#include <vector>
#include "all_module.h"
using std::locale;
using std::cout;
using std::endl;
using std::vector;

void usage() {
	fprintf(stderr,
		"Server mode:\n"
		"    ./sft.out [option]\n"
		"Client mode:\n"
		"    ./sft.out [option] [argument] ip:port\n"
		"Options: \n"
		"    General:\n"
		"        -h             This information.\n"
		"        -v             Display version.\n"
		"    In server mode:\n"
		"        -n             No log file will be created.\n"
		"    In client mode:\n"
		"        -f             File mode for sending file. Argument is your file's path.\n"
		"        -g             Fetch file from server. Argument is the file name on server.\n"
		"        -m             Message mode for sending messages. Argument is your content.\n"
		"Arguments: \n"
		"        -f             [file_path]\n"
		"        -g             [file_name]\n"
		"        -m             [contents]\n"
		"Examples:\n"
		"    ./sft.out -f ./file 255.255.255.0:8888\n"
		"    ./sft.out -g file_name 255.255.255.0:8888\n"
		"    ./sft.out -m hello,world! 255.255.255.0:8888\n"
	);
	exit(2);
}

void version() {
	cout << "Simple-File-Transfer by greatmfc\n" << "Version: " << VERSION << endl;
	cout << "Last modified date: " << LAST_MODIFIED << endl;
	cout << "Build date: " << __DATE__ << ' ' << __TIME__ << endl;
}

void check_file(char*& path) {
	if (strchr(path, '/') == NULL) {
		fprintf(stderr, "Invalid path. Please check the path.\n");
		exit(1);
	}
	if (access(path, R_OK) != 0) {
		fprintf(stderr, "Fail to access the file. Please check read permission.\n");
		exit(1);
	}
	
	struct stat st;
	stat(path, &st);
	if (!S_ISREG(st.st_mode)) {
		fprintf(stderr, "Target file is not a regular file.\n");
		exit(1);
	}
}

void parse_arg(const string_view& arg, string& ip, uint16_t& port) {
	auto idx = arg.find(':');
	if (idx == string_view::npos) {
		fprintf(stderr, "Fail to locate port number.\n");
		exit(1);
	}
	ip = arg.substr(0, idx);
	port = (uint16_t)atoi(arg.substr(idx + 1).data());
}

static void sig_hanl(int sig) {
	LOG_WARN("Receive ", strsignal(sig), ".");
#ifdef DEBUG
	cout << "\rReceived signal " << strsignal(sig) << endl;
#endif // DEBUG
	if (sig == SIGPIPE) return;
#ifdef DEBUG
	cout << "\rShutting down..." << endl;
#endif // DEBUG
	receive_loop::stop_loop(sig);
}

static void register_signal(const vector<int>& sigs) {
	for (auto& sig : sigs) {
		signal(sig, sig_hanl);
	}
}

int main(int argc, char* argv[])
{
	int opt = 0;
	bool no_log_file = false;
	char* mesg = nullptr;
	char* path = nullptr;
	char* file_to_get = nullptr;
	char mode[] = "cm:f:g:hvn";
	static vector<int> sig_to_register = { SIGINT,SIGSEGV,SIGTERM,SIGPIPE };
#ifndef __aarch64__
	locale::global(locale("en_US.UTF-8"));
#endif // !__aarch64__
	while ((opt = getopt(argc, argv, mode)) != EOF) {
		switch (opt)
		{
		case 'm':
			mesg = optarg;
			break; 
		case 'f':
			path = optarg;
			break; 
		case 'h':
			usage();
			break;
		case 'v':
			version();
			exit(2);
			break; 
		case 'n':
			log::get_instance()->no_logfile();
			no_log_file = true;
			break;
		case 'g':
			file_to_get = optarg;
			break;
		default: throw std::invalid_argument("");
		}
	}
	if (argc != 1 && optind == argc) {
		if (!no_log_file) {
			fprintf(stderr, "Missing ip address!\n");
			exit(3);
		}
	}
#ifdef DEBUG
	version();
#endif // DEBUG
	if (argc <= 2) {
		std::ios::sync_with_stdio(false);
		log::get_instance()->init_log();
		register_signal(sig_to_register);
		receive_loop rl;
		rl.loop();
	}
	else{
		string ip;
		uint16_t port = 0;
		parse_arg(argv[optind], ip, port);
		mfcslib::NetworkSocket server(ip, port);
		if (path != nullptr) {
			check_file(path);
			mfcslib::File file(path, false, 0);
			send_file_to(server, file);
		}
		else if (mesg != nullptr) {
			send_msg_to(server, mesg);
		}
		else if(file_to_get!=nullptr){
			get_file_from(server, file_to_get);
		}
	}
	cout << "Success on dealing. Please check the server." << endl;
	return 0;
}
