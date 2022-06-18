#include "common_headers.h"

log::log()
{
	time(&rawtime);
	time_info = localtime(&rawtime);
	strftime(log_name, 64, "./log_%Y-%m-%d", time_info);
	logfile_fd = fopen(log_name,"a");
	if (logfile_fd < 0) {
		perror("Open log file failed");
		exit(1);
	}
	setvbuf(logfile_fd, NULL, _IONBF, 0);
}

log::~log()
{
	flush_all_missions();
	fclose(logfile_fd);
	if (!keep_log) {
		remove(log_name);
	}
}

void log::submit_missions(string_view&& sv)
{
	container.emplace(forward<string_view>(sv));
	write_log();
}

void log::submit_missions(MyEnum&& type, const sockaddr_in& _addr, const char* msg)
{
	char content[128] = { 0 };
	time(&rawtime);
	time_info = localtime(&rawtime);
	strftime(content, 64, "%Y-%m-%d %H:%M:%S ", time_info);
	switch (type)
	{
	case FILE_TYPE: strcat(content, "File from:"); break;
	case MESSAGE_TYPE: strcat(content, "Message from:"); break;
	case ACCEPT: strcat(content, "Accept from:"); break;
	case CLOSE: strcat(content, "Close by:"); break;
	default: break;
	}
	strcat(content, inet_ntoa(_addr.sin_addr));
	strcat(content, " ");
	strcat(content, msg);
	if (strchr(content, '\n') == NULL) {
		strcat(content, "\n");
	}
	string_view sv(content);
	container.emplace(move(sv));
	write_log();
}

void log::init_logfile()
{
}

void log::write_log()
{
	while (container.size())
	{
		//locker.lock();
		fputs(container.front().data(), logfile_fd);
		container.pop();
		//locker.unlock();
	}
}

