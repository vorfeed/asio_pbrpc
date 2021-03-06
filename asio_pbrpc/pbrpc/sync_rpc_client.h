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

class SyncRPCClient : public TCPConnection<RPCBuffer>,
                  public google::protobuf::RpcChannel {
 public:
  using TCPConnection<RPCBuffer>::TCPConnection;

  SyncRPCClient(boost::asio::io_service& io_service, Executor& executor) :
    TCPConnection(io_service), executor_(executor) {}

  virtual ~SyncRPCClient() {}

  void CallMethod(const google::protobuf::MethodDescriptor* method,
      google::protobuf::RpcController* controller,
      const google::protobuf::Message* request,
      google::protobuf::Message* response,
      google::protobuf::Closure* done) override {
    size_t method_id = std::hash<std::string>()(method->full_name());
    BufferPtr output_buffer(std::make_shared<RPCBuffer>());
    output_buffer->Serialize(method_id, *request);
    if (!SyncSend(output_buffer)) {
      if (controller) {
        controller->SetFailed("send failed");
      }
      return;
    }
    while (true) {
      if (!SyncReceive()) {
        if (controller) {
          controller->SetFailed("receive failed");
        }
        return;
      }
      size_t method_id;
      boost::tribool ret = input_buffer()->Parse(method_id, *response);
      if (!ret) {
        if (controller) {
          controller->SetFailed("parse failed");
        }
        return;
      } else if (ret) {
        break;
      }
    }
  }

 private:
  Executor& executor_;
  google::protobuf::Closure* done_ { nullptr };
};

}
