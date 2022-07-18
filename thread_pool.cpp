#include "common_headers.h"

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
