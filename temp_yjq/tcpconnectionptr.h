#ifndef TINY_MUDUO_TCPCONNECTIONPTR_H_
#define TINY_MUDUO_TCPCONNECTIONPTR_H_

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <string> 

#include "callback.h"
#include "channel.h"

using std::string;

namespace tiny_muduo {

class EventLoop;

class TcpConnectionPtr {
 public:
  TcpConnectionPtr(EventLoop* loop,int connfd);
  ~TcpConnectionPtr();

  void SetConnectionCallback(const ConnectionCallback& callback) { 
    connection_callback_ = callback;
  }

  void SetMessageCallback(const MessageCallback& callback) {
    message_callback_ = callback;
  }

  void ConnectionEstablished() {
    channel_->EnableReading();
    connection_callback_(this);
  }

  void HandleMessage();
  void Send(const string& str);
  string Get();
 
 private:
  int Recv() {
    memset(buf_, '\0', sizeof(buf_));
    int ret = recv(connfd_, buf_, 100, 0);
    return ret;
  }

  EventLoop* loop_;
  int connfd_;
  Channel* channel_;
  char buf_[100] = {0};

  ConnectionCallback connection_callback_;
  MessageCallback message_callback_;
};

}  // namespace tiny_muduo
#endif

