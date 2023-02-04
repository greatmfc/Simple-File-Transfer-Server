#ifndef COHPP
#define COHPP
#include <coroutine>
using std::suspend_always;
using std::suspend_never;
namespace mfcslib {
	class co_handle
	{
	public:
		struct promise_type;
		using handle_type = std::coroutine_handle<promise_type>;
		co_handle(handle_type h) :m_co(h) {}
		co_handle(const co_handle&) = delete;
		co_handle(co_handle&& s) :m_co(s.m_co) {
			s.m_co = nullptr;
		}
		~co_handle() {
			if (m_co) {
				m_co.destroy();
			}
		}
		co_handle& operator=(const co_handle&) = delete;
		co_handle& operator=(co_handle&& s)
		{
			m_co = s.m_co;
			s.m_co = nullptr;
			return *this;
		}
		void resume() {
			m_co.resume();
		}
		bool done() {
			return m_co.done();
		}

		struct promise_type {
			auto get_return_object() {
				return co_handle(handle_type::from_promise(*this));
			}
			suspend_never initial_suspend() noexcept { return {}; }
			suspend_always final_suspend() noexcept { return {}; }
			suspend_always yield_value(int) { return {}; }
			void return_void() {}
			void unhandled_exception() {}
		};

	private:
		handle_type m_co;
	};

}
#endif // !COHPP
