#include "common_headers.h"

receive_loop::receive_loop(setup& s)
{
	pt = &s;
	addr = pt->addr;
	len = sizeof(addr);
	socket_fd = pt->socket_fd;
}

void receive_loop::loop()
{
	int ret=listen(socket_fd, 5);
	assert(ret >= 0);
	epoll_instance.add_fd_or_event_to_epoll(socket_fd,false,true,0);
	epoll_instance.set_fd_no_block(socket_fd);
	while (1) {
		int count=epoll_instance.wait_for_epoll();
		if (count < 0 && errno != EINTR) {
			perror("Error epoll_wait");
			exit(1);
		}
		for (int i = 0; i < count; ++i) {
			int react_fd = epoll_instance.events[i].data.fd;

			if (react_fd == socket_fd) {
				while (true)
				{
					int accepted_fd = accept(socket_fd, (struct sockaddr*)&addr, &len);
					if (accepted_fd < 0 && errno != EAGAIN) {
						perror("Accept failed");
						exit(1);
					}
					else if(accepted_fd<0) {
						break;
					}
				#ifdef DEBUG
					printf("Accept from client:%s\n", inet_ntoa(addr.sin_addr));
				#endif // DEBUG
					LOG_ACCEPT(addr);
					dis[accepted_fd].address = addr;
					dis[accepted_fd].fd = -1;
					epoll_instance.add_fd_or_event_to_epoll(accepted_fd, false, true, 0);
				}
			}
			else if (epoll_instance.events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
				#ifdef DEBUG
					printf("Disconnect from client:%s\n", inet_ntoa(dis[react_fd].address.sin_addr));
				#endif // DEBUG
					LOG_CLOSE(dis[react_fd].address);
				epoll_instance.remove_fd_from_epoll(react_fd);
				if (dis[react_fd].fd > 0) {
					close(dis[react_fd].fd);
				}
			}
			else if (epoll_instance.events[i].events & EPOLLIN) {
				status_code = decide_action(react_fd);
				if (status_code == FILE_TYPE) {
					deal_with_file(react_fd);
				}
				else if (status_code == MESSAGE_TYPE) {
					deal_with_mesg(react_fd);
				}
				else if (status_code == GPS_TYPE){
					deal_with_gps(react_fd);
				}
				memset(dis[react_fd].buffer_for_pre_messsage, 0, 256);
			}
		}
	}
}

int receive_loop::decide_action(int fd)
{
	char msg1 = '1';
	ssize_t ret = 0;
	ret = read(fd, dis[fd].buffer_for_pre_messsage, BUFFER_SIZE);
	if (ret < 0 && errno!=EAGAIN) {
		perror("Something happened while read from client");
		goto end;
	}
#ifdef DEBUG
	cout << "Read msg from client: " << dis[fd].buffer_for_pre_messsage << endl;
#endif // DEBUG
	write(fd, &msg1, sizeof(msg1));
	switch (dis[fd].buffer_for_pre_messsage[0])
	{
	case 'm':return MESSAGE_TYPE;
	case 'f':return FILE_TYPE;
	case 'g':return GPS_TYPE;
	default:return NONE_TYPE;
	}
	end:return -1;
}

void receive_loop::deal_with_file(int fd)
{
	char full_path[64]="./";
	char* msg = dis[fd].buffer_for_pre_messsage;
	msg += 2;
	ssize_t size = atoi(strchr(msg, '/')+1);
	strncat(full_path, msg, strcspn(msg, "/"));
	int write_fd = open(full_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	dis[fd].bytes_to_deal_with = size;
	dis[fd].buf = new char[size];
	char* buf = dis[fd].buf;
	ssize_t ret = 0;
	while (size)
	{
		ret=read(fd, buf, size);
		if (ret < 0)
		{
			perror("Receieve failed");
			exit(1);
		}
		size -= ret;
		buf += ret;
	}
	//di->fd = write_fd;
	buf = dis[fd].buf;
	for (;;) {
		ret = write(write_fd, dis[fd].buf, dis[fd].bytes_to_deal_with);
		if (ret < 0) {
			perror("Server write to local failed");
			exit(1);
		}
		dis[fd].bytes_to_deal_with -= ret;
		dis[fd].buf += ret;
		if (dis[fd].bytes_to_deal_with <= 0) {
			break;
		}
	}
#ifdef DEBUG
	cout << "Success on receiving file: " << msg << endl;
#endif // DEBUG
	LOG_FILE(dis[fd].address, move(msg));
	delete buf;
	close(write_fd);
}

void receive_loop::deal_with_mesg(int fd)
{
	char* pt = dis[fd].buffer_for_pre_messsage;
	char buffer[256]{ 0 };
	strcpy(buffer, pt+2);
	strcat(buffer, "\n");
	LOG_MSG(dis[fd].address, move(buffer));
#ifdef DEBUG
	cout << "Success on receiving message: " << buffer;
#endif // DEBUG
}

void receive_loop::deal_with_gps(int fd)
{
	char file_name[32] = "gps_";
	strcat(file_name, inet_ntoa(dis[fd].address.sin_addr));
	if (dis[fd].fd == -1) {
		dis[fd].fd = open(file_name, O_WRONLY | O_CREAT | O_APPEND, 0644);
	}
	if (dis[fd].fd < 0) {
		perror("Open file failed");
		exit(1);
	}
	char* pt = dis[fd].buffer_for_pre_messsage;
	char buffer[256]{ 0 };
	strcpy(buffer, pt+2);
	strcat(buffer, "\n");
	write(dis[fd].fd, buffer, strlen(buffer));
	LOG_MSG(dis[fd].address, "GPS content");
#ifdef DEBUG
	cout << "Success on receiving GPS: " << buffer;
#endif // DEBUG
}

void receive_loop::reset_buffers()
{
	//memset(pre_msg, 0, sizeof pre_msg);
	//memset(buffer, 0, sizeof buffer);
}

send_file::send_file(setup& s, char*& path)
{
	pt = &s;
	file_path = path;
	socket_fd = pt->socket_fd;
}

void send_file::write_to()
{
	int file_fd = open(file_path, O_RDONLY);
	if (file_fd < 0) {
		perror("Open file failed");
		exit(1);
	}
	struct stat st;
	fstat(file_fd, &st);
	ssize_t ret = 0;
	char *msg = strrchr(file_path, '/') + 1;
	strcat(msg, "/");
	strcat(msg, to_string(st.st_size).data());
	char complete_msg[] = "f/";
	strcat(complete_msg, msg);
	write(socket_fd, complete_msg, strlen(complete_msg));
	char flag = '0';
	ret = recv(socket_fd, &flag, sizeof(flag), 0);
	assert(ret >= 0);
	if (flag != '1') {
		printf("Receive flag from server failed.\n");
#ifdef DEBUG
		cout << __LINE__ << endl;
#endif // DEBUG
		exit(1);
	}
	off_t off = 0;
	long send_size = st.st_size;
	while (send_size > 0) {
		ssize_t ret = sendfile(socket_fd, file_fd, &off, send_size);
		if(ret < 0)
		{
			perror("Sendfile failed");
			break;
		}
	#ifdef DEBUG
		cout << "have send :" << ret << endl;
	#endif // DEBUG
		send_size -= ret;
	}
	close(socket_fd);
	close(file_fd);
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
	if (ret < 0) {
		perror("Binding failed");
		exit(-1);
	}
}

setup::setup(char* ip_addr,int port)
{
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	try
	{
		target_addr = inet_addr(ip_addr);
		if (target_addr == INADDR_NONE) {
			throw invalid_argument("Invalid address:");
		}
	}
	catch (const std::exception& ex)
	{
		cout << ex.what() << ip_addr << endl;
		exit(1);
	}
	memcpy(&addr.sin_addr, &target_addr, sizeof(target_addr));
	unsigned short pt = static_cast<unsigned short>(port);
	addr.sin_port = htons(pt);
	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	int ret = connect(socket_fd, (struct sockaddr*)&addr, sizeof(addr));
	if (ret < 0) {
		perror("Connect failed");
		exit(1);
	}
}

send_msg::send_msg(setup& s, char*& msg) : msg(msg)
{
	pt = &s;
	socket_fd = pt->socket_fd;
}

void send_msg::write_to()
{
	char pre_msg[256]{ 0 };
	strcpy(pre_msg, "m/");
	strcat(pre_msg, msg);
	write(socket_fd, pre_msg, strlen(pre_msg));
	close(socket_fd);
}

