#include "sft.h"

static void usage() {
	fprintf(stderr, "sft [option] [argument] ip:port\n"
		"options: \n"
		"-c				Check mode for identification before receiving.\n"
		"-f				File mode for sending file.Argument is your file's path.\n"
		"-m				Message mode for sending message.Argument is your content.\n"
		"-h				This information.\n"
		"-v				Display version.\n"
		"Example:		./sft -f ./file 255.255.255.0:8888\n"
		"				./sft -m hello,world! 255.255.255.0:8888\n");
	exit(2);
}

static void version() {
	cout << "Simple-File-Transfer by greatmfc\n" << "Version: " << VERSION << endl;
	exit(2);
}

static void check_file(char* path) {
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

static void sig_hanl(int sig) {
	cout << "\rShutting down..." << endl;
	exit(0);
}

static void parse_arg(char* arg, char* port, char* ip) {
	char* tmp = strchr(arg, ':');
	if (tmp == NULL) {
		fprintf(stderr, "Fail to locate port number.\n");
		exit(1);
	}
	tmp += 1;
	strcpy(port, tmp);
	unsigned sz = strcspn(arg, ":");
	if (ip == nullptr) {
		perror("Failed to allocate memory.\n");
		exit(1);
	}
	strncpy(ip, arg, sz);
}

int main(int argc, char* argv[])
{
	int opt = 0;
	int check_mode = 0;
	char* mesg = nullptr;
	char* path = nullptr;
	char mode[8] = "cm:f:hv";
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
			default: break;
		}
	}
	if (argc != 1) {
		if (optind == argc) {
			fprintf(stderr, "Missing ip address!\n");
			exit(3);
		}
	}
	if (path != nullptr) {
		char* ip, * port;
		ip = (char*)malloc(16);
		port = (char*)malloc(8);
		parse_arg(argv[optind], port, ip);
		check_file(path);
		setup st(ip, atoi(port));
		send_file sf(&st, path);
		sf.write_to();
		free(ip);
		free(port);
	}
	else if (mesg != nullptr)
	{
		char* ip, * port;
		ip = (char*)malloc(16);
		port = (char*)malloc(8);
		parse_arg(argv[optind], port, ip);
		setup st(ip, atoi(port));
		send_msg sm(&st, mesg);
		sm.write_to();
		free(ip);
		free(port);
	}
	else
	{
		signal(SIGINT, sig_hanl);
		setup st;
		receive_loop rl(&st);
		rl.loop();
	}
	cout << "Success on sending. Please check the server." << endl;
	return 0;
}