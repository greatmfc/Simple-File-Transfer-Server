#include "common_headers.h"

static void usage() {
	fprintf(stderr, "sft [option] [argument] ip:port\n"
		"options: \n"
		"-c				Check mode for identification before receiving.\n"
		"-f				File mode for sending file.Argument is your file's path.\n"
		"-m				Message mode for sending message.Argument is your content.\n"
		"-h				This information.\n"
		"-v				Display version.\n"
		"-n				No log file left behind after finishing program.\n"
		"Example:		./sft -f ./file 255.255.255.0:8888\n"
		"				./sft -m hello,world! 255.255.255.0:8888\n");
	exit(2);
}

static void version() {
	cout << "Simple-File-Transfer by greatmfc\n" << "Version: " << VERSION << endl;
	exit(2);
}

static void check_file(char*& path) {
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

static void parse_arg(char*& arg, char*& port, char*& ip) {
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

static void sigint_hanl(int sig) {
	cout << "\rShutting down..." << endl;
	exit(0);
}

int main(int argc, char* argv[])
{
	int opt = 0;
	int check_mode = 0;
	bool no_log_file = false;
	char* mesg = nullptr;
	char* path = nullptr;
	char mode[] = "cm:f:hvn";
	while ((opt = getopt(argc, argv, mode)) != EOF) {
		switch (opt)
		{
		case 'c':
		{
			check_mode = 1;
			break; 
		}
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
		signal(SIGINT, sigint_hanl);
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
		delete ip;
		delete port;
	}
	cout << "Success on sending. Please check the server." << endl;
	return 0;
}