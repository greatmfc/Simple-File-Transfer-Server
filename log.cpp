#include "common_headers.h"

log::log()
{
}

log::~log()
{
	fclose(logfile_fd);
	if (!keep_log) {
		remove(log_name);
	}
}

void log::submit_missions(string&& sv)
{
	container.emplace(forward<string>(sv));
	condition_var.notify_one();
}

void log::submit_missions(MyEnum&& type, const sockaddr_in& _addr, char*&& msg)
{
	char content[128] = { 0 };
	time(&rawtime);
	time_info = localtime(&rawtime);
	strftime(content, 64, "%Y-%m-%d %H:%M:%S ", time_info);
	switch (type)
	{
	case FILE_TYPE: strcat(content, "[File]:"); break;
	case MESSAGE_TYPE: strcat(content, "[Message]:"); break;
	case ACCEPT: strcat(content, "[Accept]:"); break;
	case CLOSE: strcat(content, "[Closed]:"); break;
	default: break;
	}
	strcat(content, inet_ntoa(_addr.sin_addr));
	strcat(content, " ");
	strcat(content, msg);
	if (strchr(content, '\n') == NULL) {
		strcat(content, "\n");
	}
	container.emplace(content);
	condition_var.notify_one();
}

void log::init_log()
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
	thread log_thread(flush_all_missions);
	log_thread.detach();
}

void* log::write_log()
{
	unique_lock<mutex> locker(mt);
	while (1)
	{
		condition_var.wait(locker);
		fputs(container.front().data(), logfile_fd);
		container.pop();
	}
}

