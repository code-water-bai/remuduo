#pragma once

#include "../base/noncopyable.h"
#include "Channel.h"
#include "Socket.h"
#include "Callbacks.h"

#include <functional>


namespace new_muduo {

	class EventLoop;
	class InetAddress;


	class Acceptor {
		public:
			Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport = true);
			~Acceptor();

			void setNewConnectionCallback(const NewConnectionCallback& cb) {
				newConnectionCallback_ = cb;
			}

			void listen();

			bool listening() const { return listening_; }
		private:
			void handleRead();

			EventLoop* loop_;
			Socket acceptSocket_;
			Channel acceptChannel_;
			NewConnectionCallback newConnectionCallback_;
			bool listening_;
			int idleFd_;  
	};
};