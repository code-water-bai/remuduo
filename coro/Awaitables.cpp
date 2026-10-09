#include "Awaitables.h"
#include "CoroEventLoop.h"
#include"../net/EventLoop.h"
#include "CoroTcpConnection.h" 
#include <unistd.h>
#include <sys/socket.h>

//namespace new_muduo{
//	using namespace coro;
//	void AsyncRead::await_suspend(std::coroutine_handle<> h)
//	{
//		handle_ = h;
//		con_->channel_->setReadCallback([this](new_muduo::Timestamp) {onReadable(); });
//		con_->channel_->enableReading();
//	}
//
//	void AsyncRead::onReadable()
//	{
//		int savedErrno = 0;
//		ssize_t n = con_->inputBuffer_.readFd(con_->fd(), &savedErrno);
//		if (n > 0) {
//			bytesRead_ += n;
//			if (bytesRead_ < count_) return;
//		}
//		else if (n == 0) {
//			bytesRead_ = 0;
//		}
//		else {
//			errno = savedErrno;
//			bytesRead_ = -1;
//		}
//		con_->channel_->disableReading();
//	//	con_->channel_->setReadCallback([this](new_muduo::Timestamp) {onReadable(); });
//		handle_.resume();
//	}
//
//	void AsyncWrite::await_suspend(std::coroutine_handle<> h) {
//		handle_ = h;
//		con_->channel_->setWriteCallback([this]() { onWritable(); });
//		con_->channel_->enableWriting();
//	}
//
//	void AsyncWrite::onWritable() {
//		while (remaining_ > 0) {
//			size_t n = ::write(con_->fd(), data_, remaining_);
//			if (n > 0) {
//				remaining_ -= n;
//				data_ += n;
//				has_write_ += n;
//				if (remaining_ > 0) return ;
//			}
//			else if (n == 0) {
//				has_write_= -1;
//				break;
//			}
//			else if (n < 0) {
//				if (errno == EAGAIN) return;
//				else {
//					has_write_ = -1;
//					break;
//				}
//			}
//		}
//		con_->channel_->disableWriting();
//		handle_.resume();
//	}
//
//	void AsyncSleep::await_suspend(std::coroutine_handle<> h) {
//		CoroEventLoop* evLoop = CoroEventLoop::current();
//		evLoop->getRawLoop()->runAfter(seconds_, [h]() mutable {
//			h.resume();
//		});
//	}
//
//}