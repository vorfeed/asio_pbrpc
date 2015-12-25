// Copyright 2015, Xiaojie Chen (swly@live.com). All rights reserved.
// https://github.com/vorfeed/json
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

#pragma once

#include <memory>

#include "executor.h"

namespace asio_pbrpc {

class Executors {
 public:
  void Start(size_t loop_num = 2, size_t thread_per_loop = 2) {
    if (!executors_.empty()) {
      Stop();
    }
    executors_.resize(loop_num);
    for (auto& executor : executors_) {
      executor = std::make_unique<Executor>();
      executor->Start(thread_per_loop);
    }
  }

  void Stop() {
    for (auto& executor : executors_) {
      executor->Stop();
    }
    current_index_.store(0, std::memory_order_relaxed);
    decltype(executors_) executors;
    executors_.swap(executors);
  }

  template <class F>
  void Execute(F&& f) {
    NextExcutor().Execute(std::forward<F>(f));
  }

  template <class F>
  std::future<typename std::result_of<F()>::type> ExecuteWithFuture(F&& f) {
    return NextExcutor().ExecuteWithFuture(std::forward<F>(f));
  }

  boost::asio::io_service& io_service() { return NextExcutor().io_service(); }

  template <class T>
  boost::asio::io_service& io_service(const T& key) {
    return executors_[std::hash<T>()(key) % executors_.size()]->io_service();
  }

 private:
  Executor& NextExcutor() {
    return *executors_[current_index_.fetch_add(1,
        std::memory_order_acquire) % executors_.size()];
  }

  std::atomic_size_t current_index_ { 0 };
  std::vector<std::unique_ptr<Executor>> executors_;
};

}
