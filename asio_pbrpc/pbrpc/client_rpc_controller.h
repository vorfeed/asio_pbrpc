// Copyright 2015, Xiaojie Chen (swly@live.com). All rights reserved.
// https://github.com/vorfeed/json
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

#pragma once

#include <atomic>

#include <google/protobuf/service.h>

namespace asio_pbrpc {

class ClientRPCController : public google::protobuf::RpcController {
 public:
  virtual ~ClientRPCController() {}

  void Reset() override {
    reason_.clear();
    failed_.store(false, std::memory_order_relaxed);
    cancel_.store(false, std::memory_order_relaxed);
    cancelled_.store(false, std::memory_order_relaxed);
  }

  bool Failed() const override {
    return failed_.load(std::memory_order_acquire);
  }

  std::string ErrorText() const override {
    return reason_;
  }

  void StartCancel() override {
    cancel_.store(true, std::memory_order_release);
  }

  void SetFailed(const std::string& reason) override {
    reason_ = reason;
    failed_.store(true, std::memory_order_release);
  }

  bool IsCanceled() const override {
    return cancelled_.load(std::memory_order_acquire);
  }

  void NotifyOnCancel(google::protobuf::Closure* callback) override {
    if (!callback) {
      return;
    }
    try {
      callback->Run();
    } catch (...) {}
    cancelled_.store(true, std::memory_order_release);
  }

 private:
  std::string reason_;
  std::atomic_bool failed_ { false };
  std::atomic_bool cancel_ { false };
  std::atomic_bool cancelled_ { false };
};

}
