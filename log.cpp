#include "common_headers.h"

log::log()
{
}

log::~log()
{
	//fclose(logfile_fd);
	condition_var.notify_all();
	log_file.close();
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
	log_file.open(log_name, ios::app | ios::out);
	//logfile_fd = fopen(log_name,"a");
	if (!log_file.is_open()) {
		cout<<"Open log file failed\n";
		exit(1);
	}
	//setvbuf(logfile_fd, NULL, _IONBF, 0);
	thread log_thread(flush_all_missions);
	log_thread.detach();
}

void* log::write_log()
{
	unique_lock<mutex> locker(mt);
	while (1)
	{
		condition_var.wait(locker);
		if (container.size() == 0) {
			return nullptr;
		}
		log_file << container.front().data();
		log_file.flush();
		//fputs(container.front().data(), logfile_fd);
		container.pop();
	}
}

