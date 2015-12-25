// Copyright 2015, Xiaojie Chen (swly@live.com). All rights reserved.
// https://github.com/vorfeed/json
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

#pragma once

#include <string>
#include <memory>

#include <boost/logic/tribool.hpp>
#include <google/protobuf/message.h>
#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>

#include <asio_pbrpc/net_trans/buffer.h>

namespace asio_pbrpc {

class RPCBuffer;

typedef std::shared_ptr<google::protobuf::Message> MessagePtr;
typedef std::shared_ptr<RPCBuffer> RPCBufferPtr;

class RPCBuffer : public Buffer {
 public:
  std::pair<boost::tribool, size_t> ParseMessageLength() {
    if (!readable<size_t>()) {
      return std::make_pair(boost::indeterminate, 0);
    }
    size_t message_length = read<size_t>();
    if (message_length < sizeof(size_t)) {
      return std::make_pair(false, 0);
    }
    if (readable_bytes() < message_length) {
      return std::make_pair(boost::indeterminate, 0);
    }
    return std::make_pair(true, message_length);
  }

  size_t ParseMethodId() {
    return read<size_t>();
  }

  bool ParseMessage(google::protobuf::Message& message, size_t pb_length) {
    if (!message.ParseFromArray(read(pb_length), pb_length)) {
      return false;
    }
    return true;
  }

  boost::tribool Parse(size_t& method_id, google::protobuf::Message& message) {
    auto head = ParseMessageLength();
    if (!head.first) {
      std::cerr << "bad message!" << std::endl;
      return false;
    } else if (head.first == boost::indeterminate) {
      return boost::indeterminate;
    }
    method_id = ParseMethodId();
    size_t pb_length = head.second - sizeof(size_t);
    if (!ParseMessage(message, pb_length)) {
      std::cerr << "parse protobuf failed: " << typeid(message).name() << std::endl;
      return false;
    }
    return true;
  }

  void Serialize(size_t method_id, const google::protobuf::Message& message) {
    std::string pb(message.SerializeAsString());
    write<size_t>(sizeof(size_t) + pb.length());
    write<size_t>(method_id);
    write(pb.c_str(), pb.length());
  }
};

}
