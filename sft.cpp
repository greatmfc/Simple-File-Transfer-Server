#include "common_headers.h"

static bool running;
static int pipe_fd[2];

receive_loop::receive_loop(setup& s)
{
	pt = &s;
	len = sizeof(addr);
	socket_fd = pt->socket_fd;
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipe_fd) < 0) {
		LOG_ERROR(pt->addr);
		exit(1);
	}
}

void receive_loop::stop_loop()
{
	running = false;
	char msg = '1';
	send(pipe_fd[1], &msg, 1, 0);
}

void receive_loop::loop()
{
	running = true;
	int ret=listen(socket_fd, 5);
	assert(ret >= 0);
	epoll_instance.add_fd_or_event_to_epoll(socket_fd,false,true,0);
	epoll_instance.add_fd_or_event_to_epoll(pipe_fd[0], false, true, 0);
	epoll_instance.set_fd_no_block(socket_fd);
	signal(SIGALRM, alarm_handler);
	alarm(ALARM_TIME);
	LOG_VOID("Server starts.");
	thread_pool tp;
	tp.init_pool();
	while (running) {
		int count=epoll_instance.wait_for_epoll();
		if (count < 0 && errno != EINTR) {
			LOG_ERROR(pt->addr);
			perror("Error epoll_wait");
			exit(1);
		}
		for (int i = 0; i < count; ++i) {
			int react_fd = epoll_instance.events[i].data.fd;

			if (react_fd == socket_fd) {
				while (true)
				{
					int accepted_fd = accept(socket_fd, (struct sockaddr*)&addr, &len);
					if (accepted_fd < 0) {
						if (errno != EAGAIN) {
							LOG_ERROR(addr);
							perror("Accept failed");
							exit(1);
						}
						else {
							break;
						}
					}
				#ifdef DEBUG
					cout << "Accept from client:" << inet_ntoa(addr.sin_addr) << endl;
				#endif // DEBUG
					LOG_ACCEPT(addr);
					epoll_instance.add_fd_or_event_to_epoll(accepted_fd, true, true, 0);
					accepted_fd %= DATA_INFO_NUMBER;
					dis[accepted_fd].address = addr;
					dis[accepted_fd].reserved_var = 1;
				}
			}
			else if (epoll_instance.events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
				react_fd %= DATA_INFO_NUMBER;
				close_connection(react_fd);
			}
			else if (react_fd == pipe_fd[0]) {
				char signal='2';
				recv(pipe_fd[0], &signal, sizeof signal, 0);
				if (signal == '1') continue;
				LOG_VOID("Alarm.");
				alarm(ALARM_TIME);
			}
			else if (epoll_instance.events[i].events & EPOLLIN) {
				react_fd %= DATA_INFO_NUMBER;
				status_code = decide_action(react_fd);
				switch (status_code)
				{
				case FILE_TYPE:
					tp.submit_to_pool(&receive_loop::deal_with_file,this,react_fd);
					break;
				case MESSAGE_TYPE:
					tp.submit_to_pool(&receive_loop::deal_with_mesg,this,react_fd);
					break;
				case GPS_TYPE:
					tp.submit_to_pool(&receive_loop::deal_with_gps,this,react_fd);
					break;
				}
			}
		}
	}
	tp.shutdown_pool();
	for (auto& d : dis) {
		if (d.file_stream.is_open()) {
			d.file_stream.close();
		}
	}
	LOG_VOID("Server quits.");
	exit(0);
}

int receive_loop::decide_action(int fd)
{
	char msg1 = '1';
	ssize_t ret = 0;
	ret = read(fd, dis[fd].buffer_for_pre_messsage, BUFFER_SIZE);
	if (ret < 0 && errno != EAGAIN) {
		LOG_ERROR(dis[fd].address);
#ifdef DEBUG
		perror("Something happened while read from client");
#endif // DEBUG
		goto end;
	}
	if (dis[fd].buffer_for_pre_messsage[0] != 'f' && dis[fd].reserved_var) {
		epoll_instance.add_fd_or_event_to_epoll(fd, false, true, 0);
		dis[fd].reserved_var = 0;
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
	default:return get_prefix(fd);
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
			LOG_ERROR(dis[fd].address);
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
			LOG_ERROR(dis[fd].address);
#ifdef DEBUG
			perror("Server write to local failed");
#endif // DEBUG
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
	close_connection(fd);
	delete buf;
	close(write_fd);
	memset(dis[fd].buffer_for_pre_messsage, 0, BUFFER_SIZE);
}

void receive_loop::deal_with_mesg(int fd)
{
	char* pt = dis[fd].buffer_for_pre_messsage;
	char buffer[128]{ 0 };
	strcpy(buffer, pt+2);
	strcat(buffer, "\n");
	char code = '1';
	write(fd, &code, sizeof code);
	LOG_MSG(dis[fd].address, move(buffer));
#ifdef DEBUG
	cout << "Success on receiving message: " << buffer;
#endif // DEBUG
	memset(dis[fd].buffer_for_pre_messsage, 0, BUFFER_SIZE);
}

void receive_loop::deal_with_gps(int fd)
{
	char file_name[32] = "gps_";
	strcat(file_name, inet_ntoa(dis[fd].address.sin_addr));
	if (!dis[fd].file_stream.is_open()) {
		dis[fd].file_stream.open(file_name, ios::app | ios::out);
	}
	if (dis[fd].file_stream.fail()) {
		LOG_ERROR(dis[fd].address);
		cout<<"Open gps file failed\n";
		exit(1);
	}
	char* pt = dis[fd].buffer_for_pre_messsage;
	while (*pt == ' ') ++pt;
	char buffer[128]{ 0 };
	strcpy(buffer, pt+2);
	strcat(buffer, "\n");
	dis[fd].file_stream << buffer;
	//dis[fd].file_stream.flush();
	LOG_MSG(dis[fd].address, "GPS content");
#ifdef DEBUG
	cout << "Success on receiving GPS: " << buffer;
#endif // DEBUG
	memset(dis[fd].buffer_for_pre_messsage, 0, BUFFER_SIZE);
}

void receive_loop::close_connection(int fd)
{
#ifdef DEBUG
	cout << "Disconnect from client:" << inet_ntoa(dis[fd].address.sin_addr) << endl;
#endif // DEBUG
	LOG_CLOSE(dis[fd].address);
	epoll_instance.remove_fd_from_epoll(fd);
	if (dis[fd].file_stream.is_open()) {
		dis[fd].file_stream.close();
	}
}

int receive_loop::get_prefix(int fd)
{
	for (auto& charc : dis[fd].buffer_for_pre_messsage) {
		switch (charc)
		{
		case 'm':return MESSAGE_TYPE;
		case 'f':return FILE_TYPE;
		case 'g':return GPS_TYPE;
		default:charc = ' ';
		}
	}
	return NONE_TYPE;
}

void receive_loop::alarm_handler(int)
{
	int save_errno = errno;
	char msg = '0';
	send(pipe_fd[1], &msg, 1, 0);
	errno = save_errno;
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
		cout << "Receive flag from server failed.\n";
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
		LOG_ERROR(addr);
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
	char code = '0';
	read(socket_fd, &code, sizeof code);
	if (code != '1') {
		perror("Something wrong with the server");
	}
	close(socket_fd);
}

