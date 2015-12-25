// Copyright 2015, Xiaojie Chen (swly@live.com). All rights reserved.
// https://github.com/vorfeed/json
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

#include <asio_pbrpc/net_trans/tcp_connection.h>
#include <asio_pbrpc/net_trans/tcp_server.h>
#include "rpc_buffer.h"

namespace asio_pbrpc {

class RPCServer;

class RPCServerConnection : public TCPConnection<RPCBuffer> {
 public:
  using TCPConnection<RPCBuffer>::TCPConnection;

  virtual ~RPCServerConnection() {}

  RPCServer& server() {
    assert(server_);
    return *reinterpret_cast<RPCServer*>(server_);
  }

 protected:
  bool OnReceive() override;
};

class RPCServer : public TCPServer<RPCServerConnection> {
 public:
  using TCPServer<RPCServerConnection>::TCPServer;

  void RegisterService(std::shared_ptr<google::protobuf::Service> service) {
    const google::protobuf::ServiceDescriptor* service_descriptor = service->GetDescriptor();
    for (int i = 0; i < service_descriptor->method_count(); ++i) {
      const google::protobuf::MethodDescriptor* method_descriptor = service_descriptor->method(i);
      size_t method_id = std::hash<std::string>()(method_descriptor->full_name());
      if (methods_.count(method_id)) {
        std::cerr << "duplicated method id!" << std::endl;
        continue;
      }
      methods_.emplace(method_id, std::make_pair(service, method_descriptor));
    }
  }

 private:
  friend class RPCServerConnection;

  std::unordered_map<size_t, std::pair<std::shared_ptr<google::protobuf::Service>,
      const google::protobuf::MethodDescriptor*>> methods_;
};

bool RPCServerConnection::OnReceive() {
  auto head = input_buffer()->ParseMessageLength();
  if (!head.first) {
    std::cerr << "bad message!" << std::endl;
    return false;
  } else if (head.first == boost::indeterminate) {
    AsyncReceive();
    return true;
  }
  size_t method_id = input_buffer()->ParseMethodId();
  auto ite = server().methods_.find(method_id);
  if (ite == server().methods_.end()) {
    std::cerr << "method id " << method_id << " is not registered!" << std::endl;
    return false;
  }
  std::shared_ptr<google::protobuf::Service> service = ite->second.first;
  const google::protobuf::MethodDescriptor* method_descriptor = ite->second.second;
  MessagePtr request(service->GetRequestPrototype(method_descriptor).New());
  MessagePtr response(service->GetResponsePrototype(method_descriptor).New());
  size_t pb_length = head.second - sizeof(size_t);
  if (!input_buffer()->ParseMessage(*request, pb_length)) {
    std::cerr << "parse protobuf failed: " << typeid(*request).name() << std::endl;
    return false;
  }
  auto self(shared_from_this());
  google::protobuf::Closure* done =
      google::protobuf::NewCallback<std::pair<size_t, MessagePtr>, Ptr>(
          [](std::pair<size_t, MessagePtr> output, Ptr self) {
    BufferPtr output_buffer(std::make_shared<RPCBuffer>());
    output_buffer->Serialize(output.first, *output.second);
    self->AsyncSend(output_buffer);
  }, std::make_pair(method_id, response), self);
  service->CallMethod(method_descriptor, nullptr, request.get(), response.get(), done);
  return true;
}

}
