#include "sft.h"

static void usage() {
	fprintf(stderr, "sft [option] [argument] ip:port\n"
		"options: \n"
		"-c				Check mode for identification before receiving.\n"
		"-f				File mode for sending file.\n"
		"-m				Message mode for sending message.\n"
		"-h				This information.\n"
		"-v				Display version.\n");
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

static void parse_arg(char* arg, char* ip, char* port) {
		port = strchr(arg, ':');
		if (port == NULL) {
			fprintf(stderr, "Fail to locate port number.\n");
			exit(1);
		}
		port += 1;
		unsigned sz = strcspn(arg, ":");
		ip = (char*)malloc(sz+1);
		strncpy(ip, arg, sz);
}

int main(int argc, char* argv[])
{
	if (argc > 3) {
		usage();
	}
	int opt = 0;
	int check_mode = 0;
	char* mesg, * path;
	while ((opt = getopt(argc, argv, "cm:f:hv") != EOF)) {
		switch (opt)
		{
			case 'c': check_mode = 1; break;
			case 'm': mesg = optarg; break;
			case 'f': path = optarg; break;
			case 'h': usage(); break;
			case 'v': version(); break;
			default: break;
		}
	}
	if (optind == argc) {
		fprintf(stderr, "Missing ip address!\n");
		exit(3);
	}
	if (path != nullptr) {
		char* ip, * port;
		parse_arg(argv[optind], ip, port);
		check_file(path);
		setup st(ip, atoi(port));
		send_file sf(&st, path);
		sf.write_to();
		free(ip);
	}
	else if (mesg != nullptr)
	{
		char* ip, * port;
		parse_arg(argv[optind], ip, port);
		setup st(ip, atoi(port));
		send_msg sm(&st, mesg);
		sm.write_to();
		free(ip);
	}
	else
	{
		signal(SIGINT, sig_hanl);
		receive_loop rl;
		rl.loop();
		//setup st;
		//st.receive_loop();
	}
	cout << "Success on sending. Please check the server." << endl;
	return 0;
}