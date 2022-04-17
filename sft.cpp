#include "sft.h"

receive_loop::receive_loop(setup* s)
{
	pt = s;
	addr = pt->addr;
	len = sizeof(addr);
	fd = pt->socket_fd;
}

void receive_loop::loop()
{
	int ret = listen(fd, 5);
	assert(ret >= 0);
	while (1) {
		accepted_fd = accept(fd, (struct sockaddr*)&addr, &len);
		if (accepted_fd < 0) {
			perror("Accept failed");
			exit(1);
		}
		status_code = decide_action();
		if (status_code == 1) {
			deal_with_file();
		}
		else if (status_code == 2) {
			deal_with_mesg();
		}
	}
}

int receive_loop::decide_action()
{
	char msg[8];
	int ret = recv(accepted_fd, msg, 8, 0);
	if (ret < 0) {
		perror("Read from client failed when decide action:");
		exit(1);
	}
	char confirm_code = '1';
	write(accepted_fd, &confirm_code, sizeof(confirm_code));
	if (strcmp(msg, "file") == 0) {
		return 1;
	}
	else if (strcmp(msg, "message") == 0) {
		return 2;
	}
	return 0;
}

void receive_loop::deal_with_file()
{
	char* msg = (char*)malloc(48);
	char full_msg[64]="./";
	int ret = recv(accepted_fd, msg, 48, 0);
	if (ret < 0) {
		perror("Read from client failed.");
		exit(1);
	}
	int size = atoi(strchr(msg, ':')+1);
	strncat(full_msg, msg, strcspn(msg, ":"));
	int write_fd = open(full_msg, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	char confirm_code = '0';
	write(accepted_fd, &confirm_code, sizeof(confirm_code));
	di = new data_info();
	di->bytes_to_deal_with = size;
	di->fd = accepted_fd;
	di->buf = (char*)malloc(size);
	char* buf = di->buf;
	while (size)
	{
		ret=read(accepted_fd, buf, size);
		if (ret < 0)
		{
			perror("Receieve failed: ");
			exit(1);
		}
		size -= ret;
		buf += ret;
	}
	di->fd = write_fd;
	buf = di->buf;
	for (;;) {
		ret = write(write_fd, di->buf, di->bytes_to_deal_with);
		if (ret < 0) {
			perror("Server write to file failed.");
			exit(1);
		}
		di->bytes_to_deal_with -= ret;
		di->buf += ret;
		if (di->bytes_to_deal_with <= 0) {
			break;
		}
	}
	cout << "Success on receive file: " << msg << endl;
	free(msg);
	free(buf);
	delete di;
	close(write_fd);
	close(accepted_fd);
}

void receive_loop::deal_with_mesg()
{
	int ret = 1;
	while (ret)
	{
		ret = read(accepted_fd, buffer, BUFFER_SIZE);
	}
	strcat(buffer, "\n");
	time_t rawtime;
	struct tm* time_info;
	char log_name[64];
	time(&rawtime);
	time_info = localtime(&rawtime);
	strftime(log_name, 64, "./%Y-%m-%d", time_info);
	int log_fd = open(log_name, O_CREAT | O_APPEND | O_RDWR, 0644);
	write(log_fd, buffer, strlen(buffer));
	close(log_fd);
	close(accepted_fd);
	cout << "Success on receive message: " << buffer;
}

send_file::send_file(setup* s, char* path)
{
	pt = s;
	file_path = path;
	fd = pt->socket_fd;
}

void send_file::write_to()
{
	write(fd, "file", 5);
	int file_fd = open(file_path, O_RDONLY);
	if (file_fd < 0) {
		perror("Open file failed");
		exit(1);
	}
	struct stat st;
	fstat(file_fd, &st);
	void* buf = mmap(0, st.st_size, PROT_READ, MAP_PRIVATE, file_fd, 0);
	di=new data_info();
	di->fd = file_fd;
	di->iov.iov_base = buf;
	di->bytes_to_deal_with = st.st_size;
	di->iov.iov_len = st.st_size;
	int ret = 0;
	char flag;
	ret = recv(fd, &flag, sizeof(flag), 0);
	if (flag != '1') {
		printf("Receive flag from server failed1.\n");
		exit(1);
	}
	char *msg = strrchr(file_path, '/') + 1;
	strcat(msg, ":");
	strcat(msg, to_string(st.st_size).data());
	write(fd, msg, strlen(msg));
	ret = recv(fd, &flag, sizeof(flag), 0);
	assert(ret >= 0);
	if (flag != '0') {
		printf("Receive flag from server failed2.\n");
		exit(1);
	}
	for (;;) {
		ret = writev(fd, &di->iov, IOV_NUM);
		if (ret < 0) {
			perror("Write to socket failed");
			exit(1);
		}
		di->iov.iov_len -= ret;
		di->iov.iov_base = di->iov.iov_base + ret;
		di->bytes_to_deal_with -= ret;
		if (di->bytes_to_deal_with <= 0) {
			break;
		}
	}
	close(fd);
	if (ret < 0) {
		perror("Receive end flag failed");
	}
	munmap(buf, st.st_size);
	close(file_fd);
	delete di;
}

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
	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	int ret = connect(socket_fd, (struct sockaddr*)&addr, sizeof(addr));
	if (ret < 0) {
		perror("Connect failed");
		exit(1);
	}
}

send_msg::send_msg(setup* s, char* msg) : msg(msg)
{
	pt = s;
	fd = pt->socket_fd;
}

void send_msg::write_to()
{
	int ret = pre_action(fd, false, "message");
	write(fd, msg, strlen(msg)+1);
	close(fd);
}

int basic_action::pre_action(int fd, bool active, const char* msg)
{
	char msg1 = '0';
	int ret = 0;
	if (active) {
		ret = write(fd, &msg1, 8);
		assert(ret >= 0);
		return 1;
	}
	else
	{
		ret=write(fd, msg, sizeof(msg));
		assert(ret >= 0);
		ret = read(fd, &msg1, sizeof(msg1));
		assert(ret >= 0);
		if (msg1 != '0') {
			return 1;
		}
	}
	return 0;
}
