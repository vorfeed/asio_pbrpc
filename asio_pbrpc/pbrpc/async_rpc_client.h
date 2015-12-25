// Copyright 2015, Xiaojie Chen (swly@live.com). All rights reserved.
// https://github.com/vorfeed/json
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

#pragma once

#include <google/protobuf/service.h>

#include <asio_pbrpc/net_trans/tcp_connection.h>
#include <asio_pbrpc/net_trans/executor.h>
#include "rpc_buffer.h"

namespace asio_pbrpc {

class AsyncRPCClient : public TCPConnection<RPCBuffer>,
                  public google::protobuf::RpcChannel {
 public:
  using TCPConnection<RPCBuffer>::TCPConnection;

  AsyncRPCClient(boost::asio::io_service& io_service, Executor& executor) :
    TCPConnection(io_service), executor_(executor) {}

  virtual ~AsyncRPCClient() {}

  void CallMethod(const google::protobuf::MethodDescriptor* method,
      google::protobuf::RpcController* controller,
      const google::protobuf::Message* request,
      google::protobuf::Message* response,
      google::protobuf::Closure* done) override {
    size_t method_id = std::hash<std::string>()(method->full_name());
    BufferPtr output_buffer(std::make_shared<RPCBuffer>());
    output_buffer->Serialize(method_id, *request);
    response_ = response;
    controller_ = controller;
    done_ = done;
    try {
      promise_ = std::promise<void>();
    } catch (const std::future_error&) {}
    AsyncSend(output_buffer);
  }

  void Wait() {
    try {
      promise_.get_future().wait();
    } catch (const std::future_error&) {}
  }

 protected:
  bool DoReceive() {
    while (true) {
      size_t method_id;
      boost::tribool ret = input_buffer()->Parse(method_id, *response_);
      if (!ret) {
        if (controller_) {
          controller_->SetFailed("parse failed");
        }
        return false;
      } else if (ret) {
        break;
      }
      AsyncReceive();
    }
    if (!done_) {
      return true;
    }
    try {
      done_->Run();
    } catch (...) {
      return false;
    }
    return true;
  }

  bool OnReceive() override {
    bool ret = DoReceive();
    try {
      promise_.set_value();
    } catch (const std::future_error&) {}
    return ret;
  }

  void OnError(const std::string& error) override {
    if (controller_) {
      controller_->SetFailed(error);
    }
  }

 private:
  Executor& executor_;
  google::protobuf::Closure* done_ { nullptr };
  google::protobuf::Message* response_ { nullptr };
  google::protobuf::RpcController* controller_ { nullptr };
  std::promise<void> promise_;
};

}
