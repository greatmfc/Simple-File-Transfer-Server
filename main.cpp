#include <csignal>
#include <iostream>
#include <getopt.h>
#include <cstring>
#include <cstdio>
#include <sys/stat.h>
#include <locale>
#include <csignal>
#include "common_headers.h"
using std::locale;
using std::cout;
using std::endl;
void usage() {
	fprintf(stderr,
		"server mode:"
		"./sft.out [option]"
		"client mode:"
		"./sft.out [option] [argument] ip:port\n"
		"options: \n"
		"-c				Check mode for identification before receiving.\n"
		"-f				File mode for sending file.Argument is your file's path.\n"
		"-m				Message mode for sending message.Argument is your content.\n"
		"-h				This information.\n"
		"-v				Display version.\n"
		"-n				No log file left behind after finishing program.\n"
		"-g				Fetch file from server.\n"
		"Example:		./sft.out -f ./file 255.255.255.0:8888\n"
		"				./sft.out -m hello,world! 255.255.255.0:8888\n"
		"				./sft.out -g file_name 255.255.255.0:8888\n"
	);
	exit(2);
}

void version() {
	cout << "Simple-File-Transfer by greatmfc\n" << "Version: " << VERSION << endl;
	cout << "Last modify day: " << LAST_MODIFY << endl;
	exit(2);
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

void parse_arg(char*& arg, char*& port, char*& ip) {
	ip = new char[16];
	port = new char[8];
	memset(ip, 0, 16);
	memset(port, 0, 8);
	char* tmp = strchr(arg, ':');
	if (tmp == NULL) {
		fprintf(stderr, "Fail to locate port number.\n");
		exit(1);
	}
	tmp += 1;
	strcpy(port, tmp);
	size_t sz = strcspn(arg, ":");
	if (ip == nullptr) {
		perror("Failed to allocate memory");
		exit(1);
	}
	strncpy(ip, arg, sz);
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
		{
			mesg = optarg;
			break; 
		}
		case 'f':
		{
			path = optarg;
			break; 
		}
		case 'h':
		{
			usage();
			break; 
		}
		case 'v':
		{
			version();
			break; 
		}
		case 'n':
		{
			log::get_instance()->no_logfile();
			no_log_file = true;
			break;
		}
		case 'g':
		{
			file_to_get = optarg;
			break;
		}
			default: break;
		}
	}
	if (argc != 1 && optind == argc) {
		if (!no_log_file) {
			fprintf(stderr, "Missing ip address!\n");
			exit(3);
		}
	}
	if (argc <= 2) {
		std::ios::sync_with_stdio(false);
		log::get_instance()->init_log();
		register_signal(sig_to_register);
		/*
		signal(SIGINT, sig_hanl);
		signal(SIGSEGV, sig_hanl);
		signal(SIGTERM, sig_hanl);
		signal(SIGFPE, sig_hanl);
		*/
		setup st;
		receive_loop rl(st);
		rl.loop();
	}
	else{
		char* ip, * port;
		parse_arg(argv[optind], port, ip);
		setup st(ip, atoi(port));
		if (path != nullptr) {
			check_file(path);
			send_file sf(st, path);
			sf.write_to();
		}
		else if (mesg != nullptr) {
			send_msg sm(st, mesg);
			sm.write_to();
		}
		else if(file_to_get!=nullptr){
			get_file gf(st, file_to_get);
			gf.get_it();
		}
		delete ip;
		delete port;
	}
	cout << "Success on dealing. Please check the server." << endl;
	return 0;
}
