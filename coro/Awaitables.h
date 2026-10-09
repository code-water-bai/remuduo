//#pragma once
//
//#include "../net/Channel.h"
//#include "../net/Buffer.h"
//#include "../base/Timestamp.h"
//
//#include <coroutine>
//#include <sys/types.h>
//#include <cstddef>
//
//
//
//namespace new_muduo {
//	namespace coro { class CoroEventLoop; }
//	namespace coro {
//		class CoroTcpConnection;
//		using CoroTcpConnectionPtr = std::shared_ptr< CoroTcpConnection>;
//		class AsyncRead {
//		public:
//			AsyncRead(CoroTcpConnectionPtr con, size_t count) :con_(con), count_(count) { bytesRead_ = 0; }
//
//			bool await_ready() const noexcept { return false; }
//
//			void await_suspend(std::coroutine_handle<> h);
//			ssize_t await_resume() const { return bytesRead_; }
//
//			void onReadable();
//		private:
//			std::coroutine_handle<>handle_;
//			CoroTcpConnectionPtr con_;
//			ssize_t bytesRead_;
//			size_t count_;
//		};
//
//
//
//		class AsyncWrite {
//		public:
//			AsyncWrite(CoroTcpConnectionPtr con,const char* data,size_t len)
//				:con_(con),data_(data), remaining_(len),has_write_(0) {}
//
//			bool await_ready() const noexcept { return false; }
//
//			void await_suspend(std::coroutine_handle<> h);
//			ssize_t await_resume() const { return has_write_; }
//
//			void onWritable();
//
//		private:
//			CoroTcpConnectionPtr con_;
//			std::coroutine_handle<> handle_;
//			const char* data_;
//			size_t has_write_;
//			size_t remaining_;
//		};
//
//		class AsyncSleep {
//		public:
//			explicit AsyncSleep(double seconds) : seconds_(seconds) {}
//
//			bool await_ready() const noexcept { return false; }
//
//			void await_suspend(std::coroutine_handle<> h);
//			void await_resume() const noexcept {}
//
//		private:
//			double seconds_;
//		};
//	}
//
//}