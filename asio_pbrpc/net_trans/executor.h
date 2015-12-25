// Copyright 2015, Xiaojie Chen (swly@live.com). All rights reserved.
// https://github.com/vorfeed/json
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

#pragma once

#include <atomic>
#include <future>
#include <memory>
#include <thread>
#include <vector>

#include <boost/asio.hpp>

namespace asio_pbrpc {

class Executor {
 public:
  Executor() : work_(io_service_) {}

  Executor(const Executor&) = default;
  Executor& operator=(const Executor&) = default;

  void Start(size_t thread_num = 1) {
    if (running_.load(std::memory_order_acquire)) {
      return;
    }
    running_.store(std::memory_order_release);
    auto run = [this] {
      while (running_.load(std::memory_order_acquire)) {
        try {
          io_service_.run();
        } catch (...) {
          io_service_.reset();
        }
      }
    };
    if (!thread_num) {
      run();
      return;
    }
    threads_.clear();
    threads_.reserve(thread_num);
    for (size_t i = 0; i < thread_num; ++i) {
      threads_.emplace_back(run);
    }
  }

  void Stop() {
    running_.store(false, std::memory_order_release);
    if (threads_.empty()) {
      io_service_.stop();
      return;
    }
    for (size_t i = 0; i < threads_.size(); ++i) {
      io_service_.stop();
    }
    for (size_t i = 0; i < threads_.size(); ++i) {
      threads_[i].join();
    }
    decltype(threads_) threads;
    threads_.swap(threads);
  }

  template <class F>
  void Execute(F&& f) {
    io_service_.post(std::forward<F>(f));
  }

  template <class F>
  std::future<typename std::result_of<F()>::type> ExecuteWithFuture(F&& f) {
    typedef typename std::result_of<F()>::type R;
    typedef std::packaged_task<R()> Task;
    std::shared_ptr<Task> task(std::make_shared<Task>([f]{ return f(); }));
    auto future(std::move(task->get_future()));
    io_service_.post(std::bind(&Task::operator(), task));
    return std::move(future);
  }

  boost::asio::io_service& io_service() { return io_service_; }

 private:
  boost::asio::io_service io_service_;
  boost::asio::io_service::work work_;
  std::vector<std::thread> threads_;
  std::atomic_bool running_ { false };
};

}
