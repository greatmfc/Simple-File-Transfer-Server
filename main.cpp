#include <cstdio>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <cstring>
#include <unistd.h>
#include <cstdlib>
#include <cstdarg>
#include <sys/param.h>
#include <rpc/types.h>
#include <getopt.h>
#include <strings.h>
#include <time.h>
#include <signal.h>
#include <exception>
#include <iostream>
#include <cassert>
#include <sys/stat.h>
#include <sys/uio.h>

#define DEFAULT_PORT 9007
#define IOV_NUM 4
using namespace std;

struct data_info
{
    unsigned fd;
    unsigned bytes_to_deal_with;
    struct iovec iov;
};
class setup
{
public:
    setup();
    setup(char* ip_addr,int port);
	~setup();
    int socket_fd;
    struct sockaddr_in addr;
    void receive_loop();
    void write_to(char* path);

private:
    unsigned long target_addr;
    char* status_code;
    int write_to_sock;

};

setup::setup()
{
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY); 
	addr.sin_port = htons(DEFAULT_PORT);
    socket_fd = socket(PF_INET, SOCK_STREAM, 0);
	int flag = 1;
	setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    int ret = bind(socket_fd, (struct sockaddr *)&addr, sizeof(addr));
    assert(ret >= 0);
}

setup::setup(char* ip_addr,int port)
{
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    try
    {
        target_addr = inet_addr(ip_addr);
        if (target_addr == INADDR_NONE) {
            throw invalid_argument("Invalid address");
        }
    }
    catch (const std::exception& ex)
    {
        cout << ex.what()<<endl;
        exit(1);
    }
    memcpy(&addr.sin_addr, &target_addr, sizeof(target_addr));
    addr.sin_port = htons(port);
    socket_fd = socket(AF_INET, SOCK_STREAM, 0); //创建一个套接字
    int ret = connect(socket_fd, (struct sockaddr*)&addr, sizeof(addr));
    if (ret < 0) {
        perror("Connect failed");
        exit(1);
    }
}

setup::~setup()
{
}
void setup::receive_loop()
{
    while (1) {
        socklen_t len = sizeof(target_addr);
        int to_sock = accept(socket_fd, (struct sockaddr*)&target_addr, &len);
		if (to_sock < 0) {
			perror("Accept failed");
			exit(1);
		}
        char* msg = (char*)malloc(48);
        char full_msg[64]="./";
        int ret = recv(to_sock, msg, 48, 0);
        if (ret < 0) {
            perror("Read from client failed");
            exit(1);
        }
        int size = atoi(strchr(msg, ':')+1);
        strncat(full_msg, msg, strcspn(msg, ":"));
        int write_fd = open(full_msg, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        char confirm_code = '1';
        write(to_sock, &confirm_code, sizeof(confirm_code));
        void* buf = malloc(size);
        int tmp = size;
        data_info* di = new data_info();
        di->bytes_to_deal_with = size;
        di->fd = to_sock;
        di->iov.iov_base = buf;
        di->iov.iov_len = size;
        while (1)
        {
            ret=readv(to_sock, &di->iov, IOV_NUM);
            if (ret < 0)
            {
                perror("Receieve failed: ");
                exit(1);
            }
            di->bytes_to_deal_with -= ret;
            if (di->bytes_to_deal_with <= 0) {
                break;
            }
        }
        confirm_code = '0';
        write(to_sock, &confirm_code, sizeof(confirm_code));
        di->bytes_to_deal_with = size;
        di->fd = write_fd;
		for (;;) {
			ret = writev(write_fd, &di->iov, IOV_NUM);
			if (ret < 0) {
				perror("Server write to file failed");
				exit(1);
			}
            di->bytes_to_deal_with -= ret;
            di->iov.iov_len -= ret;
            if (di->bytes_to_deal_with <= 0) {
                break;
            }
		}
        cout << "Success on receive file: " << msg << endl;
        free(msg);
        free(buf);
        delete di;
        close(write_fd);
        close(to_sock);
    }
}
void setup::write_to(char* path)
{
    int file_fd = open(path, O_RDONLY);
    if (file_fd < 0) {
        perror("Open file failed");
        exit(1);
    }
    struct stat st;
    fstat(file_fd, &st);
    void* buf = malloc(st.st_size);
    struct data_info* di=new data_info();
    di->fd = file_fd;
    di->iov.iov_base = buf;
    di->bytes_to_deal_with = 0;
    di->iov.iov_len = st.st_size;
    int ret = 0;
    for (;;) {
		ret = readv(file_fd, &di->iov, IOV_NUM);
		if (ret < 0) {
			perror("Client read from file failed");
			exit(1);
		}
        di->bytes_to_deal_with += ret;
        if (di->bytes_to_deal_with >= st.st_size) {
            break;
        }
    }
    char *msg = strrchr(path, '/') + 1;
    strcat(msg, ":");
    strcat(msg, to_string(st.st_size).data());
    write(socket_fd, msg, strlen(msg));
    char flag;
    ret = recv(socket_fd, &flag, sizeof(flag), 0);
    if (flag != '1') {
        printf("Receive flag from server failed");
        exit(1);
    }
    for (;;) {
        ret = writev(socket_fd, &di->iov, IOV_NUM);
        if (ret < 0) {
            perror("Write to socket failed");
            exit(1);
        }
        di->bytes_to_deal_with -= ret;
		if (di->bytes_to_deal_with <= 0) {
            break;
		}
        di->iov.iov_len -= ret;
        di->iov.iov_base = di->iov.iov_base + ret;
    }
    ret = recv(socket_fd, &flag, sizeof(flag), 0);
    if (ret < 0) {
        perror("Receive end flag failed");
    }
    if (flag != '0') {
        fprintf(stderr, "Something wrong with server...\n");
    }
    free(buf);
    close(socket_fd);
    delete di;
}

static void usage() {
    fprintf(stderr, "syn [option] || ip:port [FILE_PATH]\n\
        -l|listen       To wait for files.\n\
        Sample: syn -l or syn 255.255.255.0:2333 ./any_file\n");
}

static void check_file(char* path) {
    if (strchr(path, '/') == NULL) {
        fprintf(stderr, "Invalid path.\n");
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

int main(int argc, char* argv[])
{
    if (argc < 2 || argc > 3) {
        usage();
        exit(1);
    }
    if (strcmp(argv[1],"-l") == 0) {
        setup st;
        int ret = listen(st.socket_fd, 5);
        assert(ret >= 0);
        signal(SIGINT, sig_hanl);
        st.receive_loop();
    }
    else
    {
        check_file(argv[2]);
        char* ip, * port;
        port = strchr(argv[1], ':');
        if (port == NULL) {
            fprintf(stderr, "Fail to locate port number.\n");
            exit(1);
        }
        port += 1;
        unsigned sz = strcspn(argv[1], ":");
        ip = (char*)malloc(sz+1);
        strncpy(ip, argv[1], sz);
        setup st(ip, atoi(port));
        st.write_to(argv[2]);
        free(ip);
    }
    cout << "Success on sending. Please check the server." << endl;
    return 0;
}