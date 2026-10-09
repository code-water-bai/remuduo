#pragma once
#include "../base/noncopyable.h"
#include "../pool/CoroutinePool.h"

#include <coroutine>
#include <exception>
#include <utility>
#include <assert>
#include <optional>

namespace new_muduo {
	using namespace pool;

	template<class T = void>
	class Task;

	namespace detail {
		struct SymmetricTransfer final {
			std::coroutine_handle<> continuation;
			explicit SymmetricTransfer(std::coroutine_handle<> h) : continuation(h) {}
			bool await_ready() const noexcept { return false; }
			std::coroutine_handle<> await_suspend(std::coroutine_handle<> caller) noexcept {
				return continuation;
			}
			void await_resume() noexcept {}
		};

		struct TaskPromiseBase {
			std::coroutine_handle<> continuation_;
			std::exception_ptr eptr_;

			TaskPromiseBase() : continuation_(std::noop_coroutine()) {}
			auto initial_suspend() { return std::suspend_always{}; }
			void unhandled_exception() {
				eptr_ = std::current_exception();
			}
		};

		template<class T>
		struct TaskPromise final : TaskPromiseBase {
			std::optional<T> result_;

			Task<T> get_return_object();

			void return_value(T val) {
				result_.emplace(std::move(val));
			}

			auto final_suspend() noexcept {
				return SymmetricTransfer{ continuation_ };
			}

			T get_result() {
				if (eptr_) {
					std::rethrow_exception(eptr_);
				}
				assert(result_.has_value());
				return std::move(*result_);
			}

			static void* operator new(std::size_t size) {
				void* ptr = CoroutinePool::instance().allocate(size);
				if (!ptr) {
					return ::operator new(size);
				}
				return ptr;
			}

			static void operator delete(void* ptr, std::size_t size) {
				CoroutinePool::instance().deallocate(ptr, size);
			}
		};

	}

	template <typename T>
	class Task {
	public:
		using promise_type = detail::TaskPromise<T>;
		Task() : handle_(nullptr) {}

		explicit Task(std::coroutine_handle<promise_type> h) : handle_(h) {}

		Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}

		Task& operator=(Task&& other) noexcept {
			if (this != &other) {
				destroy();
				handle_ = std::exchange(other.handle_, nullptr);
			}
			return *this;
		}

		Task(const Task&) = delete;
		Task& operator=(const Task&) = delete;

		~Task() { destroy(); }

		auto operator co_await() noexcept {
			struct Awaiter {
				std::coroutine_handle<promise_type> handle_;

				bool await_ready() const noexcept {
					return handle_.done();
				}

				std::coroutine_handle<> await_suspend(
					std::coroutine_handle<> continuation) noexcept {
					handle_.promise().continuation_ = continuation;
					return handle_;
				}

				T await_resume() {
					return handle_.promise().get_result();
				}
			};
			return Awaiter{ handle_ };
		}

		explicit operator bool() const { return handle_ != nullptr; }
		auto handle() const { return handle_; }
	private:
		void destroy() {
			if (handle_) {
				handle_.destroy();
				handle_ = nullptr;
			}
		}

		std::coroutine_handle<promise_type> handle_;

	};


	namespace detail {
		template <>
		struct TaskPromise<void> final : TaskPromiseBase {
			Task<void> get_return_object() {
				return Task<void>{std::coroutine_handle<TaskPromise<void>>::from_promise(*this)};
			}

			void return_void() {}

			auto final_suspend() noexcept {
				return SymmetricTransfer{ continuation_ };
			}

			void get_result() {
				if (eptr_) {
					std::rethrow_exception(eptr_);
				}
			}
		};

		template <typename T>
		Task<T> TaskPromise<T>::get_return_object() {
			return Task<T>{std::coroutine_handle<TaskPromise<T>>::from_promise(*this)};
		}

	}

}
