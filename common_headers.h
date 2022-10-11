#ifndef CH_H
#define CH_H

#include <fstream>
#include <queue>
#include <thread>
#include <future>
#include <condition_variable>
#include <functional>
#include <netinet/in.h>
#include <sys/epoll.h>

#define DEFAULT_PORT 9007
#define IOV_NUM 1
#define VERSION 1.1011
#define BUFFER_SIZE 128
#define BACKLOG 1024
#define IOURING_QUEUE_DEPTH 512
#define EPOLL_EVENT_NUMBER 1024
#define ALARM_TIME 300

using std::mutex;
using std::queue;
using std::invoke_result_t;
using std::decay_t;
using std::vector;
using std::move;
using std::forward;
using std::future;
using std::function;
using std::condition_variable;
using std::string_view;
using std::thread;
using std::ofstream;
using std::unordered_map;
using std::unique_lock;
using std::make_shared;
using std::packaged_task;
using std::bind;
using std::string;

enum MyEnum
{
	FILE_TYPE,
	MESSAGE_TYPE,
	NONE_TYPE,
	ERROR_TYPE,
	GPS_TYPE,
	ACCEPT,
	CLOSE,
	READ_PRE,
	READ_MSG,
	READ_DATA,
	WRITE_DATA,
	GET_TYPE
};

enum log_enum
{
	LINFO,
	LDEBUG,
	LWARN,
	LERROR
};

typedef struct conn_info //存放文件描述符和文件类型
{
	unsigned fd;
	unsigned type;
} conn_info;

typedef struct data_info
{
	int reserved_var[8];
	ssize_t bytes_to_deal_with;
	char* buf;
	char buffer_for_pre_messsage[BUFFER_SIZE];
	struct iovec iov;
	sockaddr_in address;
} data_info;

template <typename T>
class sync_queue
{
public:
	sync_queue()=default;
	sync_queue(sync_queue&& other)=default;
	~sync_queue()=default;

	bool is_empty() { 
		unique_lock<mutex> lock(locker_mutex);
		return container.empty(); 
	}

	int size() {
		unique_lock<mutex> lock(locker_mutex);
		return container.size(); 
	}

	void push(T& t) {
		unique_lock<mutex> lock(locker_mutex);
		container.emplace(t);
	}

	bool pop(T& t) {
		unique_lock<mutex> lock(locker_mutex);
		if (container.empty()) return false;
		t = move(container.front());
		container.pop();
		return true;
	}

private:
	queue<T> container;
	mutex locker_mutex;
};

class thread_pool
{
public:
	thread_pool(const int number_of_threads = 4) :m_threads(vector<thread>(number_of_threads)) {};
	thread_pool(const thread_pool&) = delete;
	thread_pool(thread_pool&&) = delete;
	thread_pool& operator=(const thread_pool&) = delete;
	thread_pool& operator=(thread_pool&&) = delete;
	~thread_pool() { shutdown_pool(); };
	void init_pool();
	void shutdown_pool();

	template <typename F,typename... Args,typename R=invoke_result_t<decay_t<F>,decay_t<Args>...>>
	future<R> submit_to_pool(F&& f, Args&& ...args) {
		function<R()> func_bind = bind(forward<F>(f), forward<Args>(args)...);
		//contains functions that returns type R with no extra argument
		auto task_ptr = make_shared<packaged_task<R()>>(func_bind);
		//probably with implicit conversion
		function<void()> wrapper_func = [task_ptr]() {
			(*task_ptr)();
		}; //optional
		//this wrapper_func contains a lambda function
		//that captures a shared_ptr and execute after dereference it
		m_queue.push(wrapper_func);
		m_cv.notify_one();
		return task_ptr->get_future();
	}

private:
	vector<thread> m_threads;
	sync_queue<function<void()>> m_queue;
	bool m_shutdown = false;
	mutex m_mutex;
	condition_variable m_cv;

	class thread_pool_worker
	{
	public:
		thread_pool_worker(thread_pool* tp, const int _id) :m_pool(tp), id(_id) {};
		void operator()();
	private:
		thread_pool* m_pool;
		int id = 0;
	};

};

class log
{
public:
	~log();

	template<typename ...Args>
	void process_and_submit(log_enum&& type, const Args& ...args) {
		time(&rawtime);
		time_info = localtime(&rawtime);
		char tmp[32]{ 0 };
		strftime(tmp, 32, "%Y-%m-%d %H:%M:%S ", time_info);
		string content(tmp);
		switch (type)
		{
		case LINFO:content += "[Info]:"; break;
		case LDEBUG:content += "[Debug]:"; break;
		case LWARN:content += "[Warn]:"; break;
		case LERROR:content += "[Error]:"; break;
		}
		content = (content + ... + args);
		content += '\n';
		m_submit_missions(content);
	}

	void init_log();
	static inline log* get_instance() {
		static log log_object;
		return &log_object;
	}
	constexpr void no_logfile() {
		keep_log = false;
	};

private:
	log();
	time_t rawtime;
	struct tm* time_info;
	ofstream log_file;
	bool keep_log = true;
	char log_name[64] = { 0 };
	void m_submit_missions(const string& ct);
};

class epoll_utility
{
public:
	epoll_utility();
	~epoll_utility();
	void add_fd_or_event_to_epoll(int fd, bool one_shot, bool use_et, int ev);
	int wait_for_epoll();
	int set_fd_no_block(int fd);
	void remove_fd_from_epoll(int fd);
	epoll_event events[EPOLL_EVENT_NUMBER];

private:
	int epoll_fd;
};

class setup
{
public:
	setup();
	setup(char* ip_addr,int port);
	~setup() = default;
	int socket_fd;
	struct sockaddr_in addr;

private:
	unsigned long target_addr;
	int write_to_sock;
};

class basic_action
{
protected:
	basic_action() = default;
	~basic_action() = default;
	int socket_fd;
	int status_code;
	setup* pt;
};

class receive_loop : public basic_action
{
public:
	receive_loop() = default;
	receive_loop(setup& s);
	~receive_loop() = default;
	static void stop_loop();
	void loop();

private:
	struct sockaddr_in addr;
	socklen_t len;
	epoll_utility epoll_instance;
	unordered_map<int, data_info> connection_storage;
	unordered_map<unsigned int, ofstream*> addr_to_stream;
	static inline bool running;
	static inline int pipe_fd[2];

	int decide_action(int fd);
	void deal_with_file(int fd);
	void deal_with_mesg(int fd);
	void deal_with_gps(int fd);
	void deal_with_get_file(int fd);
	void close_connection(int fd);
	int get_prefix(int fd);
	static void alarm_handler(int sig);
};

class send_file : public basic_action
{
public:
	send_file() = default;
	send_file(setup& s, char*& path);
	~send_file() = default;
	void write_to();

private:
	char* file_path;
};

class send_msg : public basic_action
{
public:
	send_msg() = default;
	send_msg(setup& s, char*& msg);
	void write_to();
	~send_msg() = default;
private:
	char* msg;
};

class get_file : public basic_action
{
public:
	get_file() = delete;
	get_file(setup& s, string_view&& msg);
	~get_file() = default;
	void get_it();

private:
	string_view file_name;

};

#define ADDRSTR(_addr) inet_ntoa(_addr.sin_addr)
#define LOG_INFO(...) log::get_instance()->process_and_submit(LINFO,__VA_ARGS__)
#define LOG_DEBUG(...) log::get_instance()->process_and_submit(LDEBUG,__VA_ARGS__)
#define LOG_WARN(...) log::get_instance()->process_and_submit(LWARN,__VA_ARGS__)
#define LOG_ERROR(...) log::get_instance()->process_and_submit(LERROR,__VA_ARGS__)

#define LOG_MSG(_addr,_msg) LOG_INFO("Message from:",ADDRSTR(_addr),' ',_msg)
#define LOG_FILE(_addr,_msg) LOG_INFO("File request from:",ADDRSTR(_addr),' ',_msg)
#define LOG_ACCEPT(_addr) LOG_INFO("Accept from:",ADDRSTR(_addr))
#define LOG_CLOSE(_addr) LOG_INFO("Close by:",ADDRSTR(_addr))
#define LOG_ERROR_C(_addr) LOG_ERROR("client:",ADDRSTR(_addr),' ',strerror(errno))

#endif //CH_H
