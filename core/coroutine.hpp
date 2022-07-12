#pragma once
#include <st.h>
#include <cassert>
#include <memory>
#include <tuple>
#include <cerrno>
#include <chrono>
#include <tuple>
#include <functional>
#include "core/logging.hpp"
namespace st {
	static bool enable_coroutine() {
		if (st_init() < 0) {
			printf("error!");
			return false;
		}
		return true;
	}

	class coroutine {
	public:
		struct _State
		{
			virtual ~_State() {}
			virtual void _M_run() = 0;
		};

		using _State_ptr = std::unique_ptr<_State>;
		typedef st_thread_t	native_handle_type;
	private:
		class id
		{
			native_handle_type	_M_coroutine;
		public:
			id() noexcept : _M_coroutine(){ }
			explicit id(native_handle_type handle) : _M_coroutine(handle){}
		private:
			friend class coroutine;
			friend class std::hash<coroutine::id>;
			friend bool operator==(coroutine::id __x, coroutine::id __y) noexcept {
				return __x._M_coroutine == __y._M_coroutine;
			}

			friend bool operator<(coroutine::id __x, coroutine::id __y) noexcept {
				return __x._M_coroutine < __y._M_coroutine;
			}

			template<class _CharT, class _Traits>
			friend std::basic_ostream<_CharT, _Traits>& operator<<(std::basic_ostream<_CharT, _Traits>& __out, coroutine::id __id) { __out << reinterpret_cast<long>(__id._M_coroutine); return __out;}
		};

	private:
		id				_M_id;

	public:
		coroutine() noexcept = default;

		coroutine(coroutine&) = delete;

		coroutine(const coroutine&) = delete;

		coroutine(const coroutine&&) noexcept;

		coroutine(coroutine&& __t) noexcept
		{
			swap(__t);
		}

		coroutine& operator=(coroutine&& _Other) noexcept {
			std::swap(_M_id, _Other._M_id);
			return *this;
		}

		template< typename _Callable, typename... _Args >
		explicit coroutine(_Callable&& __f, _Args&&... __args) {
			// 启动线程
			_M_start_coroutine(_S_make_state(__make_invoker(std::forward<_Callable>(__f), std::forward<_Args>(__args)...)));
		}

		~coroutine() {
			if (_M_id._M_coroutine) {
				void* res = NULL;
				int r0 = st_thread_join(_M_id._M_coroutine, &res);
				if (r0) {
					// By st_thread_join
					if (errno == EINVAL) {LOG(TRACE) << "1";assert(!r0);}
					if (errno == EDEADLK) { LOG(TRACE) << "1"; assert(!r0);}
					// By st_cond_timedwait
					if (errno == EINTR) { LOG(TRACE) << "1"; assert(!r0);}
					if (errno == ETIME) { LOG(TRACE) << "1"; assert(!r0);}
					// Others
					assert(!r0);
				}
				//st_thread_exit(NULL);
			}
			
			_M_id._M_coroutine = nullptr;
		}

		coroutine& operator=(const coroutine&) = delete;

		void swap(coroutine& __t) noexcept
		{
			std::swap(_M_id, __t._M_id);
		}

		coroutine::id get_id() const noexcept
		{
			return _M_id;
		}

		void terminate() {
			if(_M_id._M_coroutine)
				st_thread_interrupt(_M_id._M_coroutine);
		}
	private:
		template<typename _Callable>
		struct _State_impl : public _State {
			_Callable		_M_func;	// 线程入口函数

			_State_impl(_Callable&& __f) : _M_func(std::forward<_Callable>(__f))
			{ }

			void _M_run() { _M_func(); } // 执行线程入口函数
		};

		// 传入_Invoker对象，返回 _State_ptr 对象
		template<typename _Callable>
		static _State_ptr _S_make_state(_Callable&& __f) {
			using _Impl = _State_impl<_Callable>;
			// 使用子类对象来初始化父类
			return _State_ptr{ new _Impl{std::forward<_Callable>(__f)} };
		}

		void _M_start_coroutine(_State_ptr state)
		{
			_M_id._M_coroutine = __gthread_coroutine(&execute_native_coroutine_routine, state.get(), true, 0);
			if (_M_id._M_coroutine == nullptr)
				throw std::runtime_error("__gthread_coroutine failed");
			state.release();
		}

		// 内部调用的是 pthread_create 函数
		static inline native_handle_type __gthread_coroutine(void* (*__func) (void*), void* __args, int joinable, int stack_size)
		{
			return st_thread_create(__func, __args, joinable, stack_size);
		}

		// 内部执行线程入口函数
		static void* execute_native_coroutine_routine(void* __p)
		{
			coroutine::_State_ptr __t{ static_cast<coroutine::_State*>(__p) };
			__t->_M_run();		// 运行线程入口函数
			return nullptr;
		}

	private:
		// A call wrapper that does INVOKE(forwarded tuple elements...)
		template<typename _Tuple>
		struct _Invoker
		{
			_Tuple _M_t;

			template<size_t _Index>
			static std::tuple_element_t<_Index, _Tuple>&&_S_declval();

			template<size_t... _Ind>
			auto
				_M_invoke(std::index_sequence<_Ind...>)
				noexcept(noexcept(std::invoke(_S_declval<_Ind>()...)))
				-> decltype(std::invoke(_S_declval<_Ind>()...))
			{
				return std::invoke(std::get<_Ind>(std::move(_M_t))...);
			}

			using _Indices = typename std::make_index_sequence<std::tuple_size<_Tuple>::value>;

			auto
				operator()()
				noexcept(noexcept(std::declval<_Invoker&>()._M_invoke(_Indices())))
				-> decltype(std::declval<_Invoker&>()._M_invoke(_Indices()))
			{
				return _M_invoke(_Indices());
			}
		};

		template<typename... _Tp>
		using __decayed_tuple = std::tuple<typename std::decay<_Tp>::type...>;

	public:
		// Returns a call wrapper that stores
		// tuple{DECAY_COPY(__callable), DECAY_COPY(__args)...}.
		template<typename _Callable, typename... _Args>
		static _Invoker<__decayed_tuple<_Callable, _Args...>>
			__make_invoker(_Callable&& __callable, _Args&&... __args)
		{
			return { __decayed_tuple<_Callable, _Args...>{
				std::forward<_Callable>(__callable), std::forward<_Args>(__args)...
			} };
		}
	};

	namespace this_coroutine {
		template<typename _Rep, typename _Period>
		inline void
			sleep_for(const std::chrono::duration<_Rep, _Period>& __rtime)
		{
			if (__rtime <= __rtime.zero())
				return;
			auto __mms = std::chrono::duration_cast<std::chrono::microseconds>(__rtime);
			st_usleep(__mms.count());
		}

		inline long get_id() {
			return reinterpret_cast<long>(st_thread_self());
		}

		inline void yield() {
			st_thread_yield();
		}
	}

	class mutex {
	public:
		using  native_handle = st_mutex_t;
		mutex() {
			handle_ = nullptr;
			handle_ = st_mutex_new();
			if (handle_ == nullptr) {
				throw std::runtime_error("st_mutex_new failed");
			}
		}

		~mutex() {
			if (handle_ != nullptr) {
				st_mutex_destroy(handle_);
				handle_ = nullptr;
			}
		}

		mutex(const mutex&) = delete;
		mutex& operator=(const mutex&) = delete;

		void lock() {
			st_mutex_lock(handle_);
		}

		bool try_lock() {
			return 0 == st_mutex_trylock(handle_);
		}

		void unlock() {
			st_mutex_unlock(handle_);
		}

	private:
		native_handle handle_;
	};

	enum class cv_status { no_timeout, timeout, interrupted};

	class condition_variable
	{
	public:
		using  native_handle_type = st_cond_t;
	private:
		typedef std::chrono::system_clock	__clock_t;
		native_handle_type _M_cond;
	public:

		condition_variable() noexcept {
			_M_cond = nullptr;
			_M_cond = st_cond_new();
		}

		~condition_variable() noexcept {
			if (_M_cond != nullptr) {
				st_cond_destroy(_M_cond);
				_M_cond = nullptr;
			}
		}

		condition_variable(const condition_variable&) = delete;
		condition_variable& operator=(const condition_variable&) = delete;

		void notify_one() noexcept {
			st_cond_signal(_M_cond);
		}

		void notify_all() noexcept {
			st_cond_broadcast(_M_cond);
		}

		void wait() noexcept
		{
			st_cond_wait(_M_cond);
		}

		template<typename _Predicate>
		void wait(_Predicate __p)
		{
			while (!__p())
				wait();
		}

		template<typename _Duration>
		cv_status
			wait_until(const std::chrono::time_point<__clock_t, _Duration>& __atime)
		{
			return __wait_until_impl(__atime);
		}

		template<typename _Clock, typename _Duration>
		cv_status
			wait_until(const std::chrono::time_point<_Clock, _Duration>& __atime)
		{
			// DR 887 - Sync unknown clock to known clock.
			const typename _Clock::time_point __c_entry = _Clock::now();
			const __clock_t::time_point __s_entry = __clock_t::now();
			const auto __delta = __atime - __c_entry;
			const auto __s_atime = __s_entry + __delta;

			return __wait_until_impl(__s_atime);
		}

		template<typename _Clock, typename _Duration, typename _Predicate>
		bool
			wait_until(const std::chrono::time_point<_Clock, _Duration>& __atime,
				_Predicate __p)
		{
			while (!__p())
				if (wait_until( __atime) == cv_status::timeout)
					return __p();
			return true;
		}

		template<typename _Rep, typename _Period>
		cv_status wait_for(const std::chrono::duration<_Rep, _Period>& __rtime)
		{
			using __dur = typename __clock_t::duration;
			auto __reltime = std::chrono::duration_cast<__dur>(__rtime);
			if (__reltime < __rtime)
				++__reltime;
			return wait_until(__clock_t::now() + __reltime);
		}

		template<typename _Rep, typename _Period, typename _Predicate>
		bool
			wait_for(const std::chrono::duration<_Rep, _Period>& __rtime,
				_Predicate __p)
		{
			using __dur = typename __clock_t::duration;
			auto __reltime = std::chrono::duration_cast<__dur>(__rtime);
			if (__reltime < __rtime)
				++__reltime;
			return wait_until( __clock_t::now() + __reltime, std::move(__p));
		}

		native_handle_type
			native_handle()
		{
			return _M_cond;
		}

	private:
		template<typename _Dur>
		cv_status
			__wait_until_impl(const std::chrono::time_point<__clock_t, _Dur>& __atime)
		{
			int re = st_cond_timedwait(_M_cond, std::chrono::duration_cast<std::chrono::microseconds>(__atime - __clock_t::now()).count());
			if (re == -1) {
				if (errno == ETIME)
					return cv_status::timeout;
				else if(errno == EINTR)
					return cv_status::interrupted;
			}

            return cv_status::no_timeout;
		}
	};
}