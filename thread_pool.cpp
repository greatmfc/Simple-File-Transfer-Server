#ifdef ENABLE_MODULES
module;
#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <functional>
#include <future>
#include <memory>
#include <vector>
#include <type_traits>
export module thpool;

using std::mutex;
using std::unique_lock;
using std::queue;
using std::forward;
using std::condition_variable;
using std::thread;
using std::future;
using std::function;
using std::bind;
using std::make_shared;
using std::packaged_task;
using std::vector;
using std::invoke_result_t;
using std::decay_t;

export{
	template <typename T>
	class sync_queue
	{
	public:
		sync_queue() = default;
		sync_queue(sync_queue&& other) = default;
		~sync_queue() = default;

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

		template <typename F, typename... Args, typename R = invoke_result_t<decay_t<F>, decay_t<Args>...>>
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

}
#else
#include "common_headers.h"
#endif // __cplusplus > 201703L

void thread_pool::init_pool()
{
	int i = 0;
	for (auto& td : m_threads) {
		td = thread(thread_pool_worker(this, i));
		++i;
	}
}

void thread_pool::shutdown_pool()
{
	m_shutdown = true;
	m_cv.notify_all();
	for (auto& td : m_threads) {
		if (td.joinable()) {
			td.join();
		}
	}
}

void thread_pool::thread_pool_worker::operator()() {
	function<void()> tmp_func;
	bool already_poped = false;
	while (!m_pool->m_shutdown){
		{
			unique_lock<mutex> lock(m_pool->m_mutex);
			if (m_pool->m_queue.is_empty()) {
				m_pool->m_cv.wait(lock);
			}
			already_poped = m_pool->m_queue.pop(tmp_func);
		} 
		//free the lock in advance to avoid holding it
		//for too long during the function below is processing
		if (already_poped) tmp_func();
	}
}
