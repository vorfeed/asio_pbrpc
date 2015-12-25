// Copyright 2015, Xiaojie Chen (swly@live.com). All rights reserved.
// https://github.com/vorfeed/json
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <string>

#include <boost/asio.hpp>
#include <boost/asio/use_future.hpp>

#include "buffer.h"
#include "chrono_timer.h"

namespace asio_pbrpc {

template <class InputBuffer>
class TCPConnection : public std::enable_shared_from_this<TCPConnection<InputBuffer>> {
 public:
  static_assert(std::is_base_of<Buffer, InputBuffer>::value, "");

  typedef std::shared_ptr<TCPConnection> Ptr;
  typedef std::shared_ptr<InputBuffer> BufferPtr;
  typedef std::weak_ptr<InputBuffer> BufferWeakPtr;

  TCPConnection(boost::asio::io_service& io_service, void* server = nullptr) :
    io_service_(io_service), socket_(io_service_), server_(server), timer_(io_service_) {}
  virtual ~TCPConnection() { Close(); }

  bool Bind(const boost::asio::ip::tcp::endpoint& socket) {
    try {
      if (socket_.is_open()) {
        socket_.close();
      }
      socket_.open(boost::asio::ip::tcp::v4());
      socket_.bind(socket);
    } catch (const boost::system::system_error& se) {
      std::cerr << "bind failed: " << se.what() << std::endl;
      return false;
    }
    local_ = socket_.local_endpoint();
    return true;
  }

  void AsyncConnect(const boost::asio::ip::tcp::endpoint& remote) {
    remote_ = remote;
    Expire(connect_timeout_);
    auto self(this->shared_from_this());
    socket_.async_connect(remote,
        [this, self](const boost::system::error_code& ec) {
      Cancel(connect_timeout_);
      if (ec) {
        std::cerr << "connect failed, local(" <<
            socket_.local_endpoint().address().to_string() << ":" <<
            socket_.local_endpoint().port() << "), remote(" <<
            remote_.address().to_string() << ":" <<
            remote_.port() << "): " << ec.message() << std::endl;
        OnError("connect failed");
        return;
      }
      local_ = socket_.local_endpoint();
      OnConnect();
    });
  }
  void AsyncConnect(const std::string& host, int port) {
    AsyncConnect(boost::asio::ip::tcp::endpoint(
        boost::asio::ip::address::from_string(host), port));
  }

  bool SyncConnect(const boost::asio::ip::tcp::endpoint& remote) {
    remote_ = remote;
    Expire(connect_timeout_);
    try {
      socket_.connect(remote);
    } catch (const boost::system::system_error& se) {
      std::cerr << "connect failed, local(" <<
          socket_.local_endpoint().address().to_string() << ":" <<
          socket_.local_endpoint().port() << "), remote(" <<
          remote.address().to_string() << ":" <<
          remote.port() << "): " << se.what() << std::endl;
      return false;
    }
    Cancel(connect_timeout_);
    local_ = socket_.local_endpoint();
    return true;
  }
  bool SyncConnect(const std::string& host, int port) {
    return SyncConnect(boost::asio::ip::tcp::endpoint(
        boost::asio::ip::address::from_string(host), port));
  }

  void Connect(const boost::asio::ip::tcp::endpoint& remote) {
    remote_ = remote;
    connect_future_ = socket_.async_connect(remote, boost::asio::use_future);
    if (connect_timeout_ > std::chrono::milliseconds::zero()) {
      last_time_ = now();
    }
  }
  void Connect(const std::string& host, int port) {
    SyncConnect(boost::asio::ip::tcp::endpoint(
        boost::asio::ip::address::from_string(host), port));
  }

  bool WaitForConnect() {
    return ConnectFutureWait();
  }

  bool Start() {
    socket_.non_blocking(true);
    socket_.set_option(boost::asio::ip::tcp::no_delay(true));
    socket_.set_option(boost::asio::socket_base::reuse_address(true));
    local_ = socket_.local_endpoint();
    remote_ = socket_.remote_endpoint();
    if (!OnConnect()) {
      Close();
      return false;
    }
    return true;
  }

  void AsyncSend(BufferPtr output_buffer) {
    assert(output_buffer->readable_bytes());
    Expire(send_timeout_);
    auto self(this->shared_from_this());
    socket_.async_write_some(boost::asio::buffer(output_buffer->read_buffer(),
        output_buffer->readable_bytes()),
        [this, self, output_buffer](const boost::system::error_code& ec,
            size_t bytes_transferred) {
      Cancel(send_timeout_);
      if (ec) {
          std::cerr << "send failed: " << ec.message() << std::endl;
          Close();
          OnError("send failed");
          return;
      }
      std::cout << bytes_transferred << " byte(s) sent." << std::endl;
      output_buffer->retrieve(bytes_transferred);
      if (!OnSend()) {
        Close();
        return;
      }
      AsyncReceive();
    });
  }

  bool SyncSend(BufferPtr output_buffer) {
    assert(output_buffer->readable_bytes());
    Expire(send_timeout_);
    boost::system::error_code ec;
    while (output_buffer->readable_bytes()) {
      size_t bytes_transferred = socket_.write_some(boost::asio::buffer(
          output_buffer->read_buffer(), output_buffer->readable_bytes()), ec);
      if (ec) {
        std::cerr << "send failed: " << ec.message() << std::endl;
        Close();
        return false;
      }
      output_buffer->retrieve(bytes_transferred);
    }
    Cancel(send_timeout_);
    return true;
  }

  void Send(BufferPtr output_buffer) {
    assert(output_buffer->readable_bytes());
    future_ = socket_.async_write_some(boost::asio::buffer(output_buffer->read_buffer(),
        output_buffer->readable_bytes()), boost::asio::use_future);
    output_buffer_ = output_buffer;
    if (send_timeout_ > std::chrono::milliseconds::zero()) {
      last_time_ = now();
    }
  }

  bool WaitForSend() {
    std::size_t bytes_transferred = FutureWait("send", send_timeout_);
    if (!bytes_transferred) {
      return false;
    }
    if (BufferPtr output_buffer = output_buffer_.lock()) {
      output_buffer->retrieve(bytes_transferred);
    }
    return true;
  }

  void AsyncReceive() {
    Expire(receive_timeout_);
    auto self(this->shared_from_this());
    socket_.async_read_some(boost::asio::buffer(input_buffer_->write_buffer(),
        input_buffer_->writable_bytes()),
        [this, self](const boost::system::error_code& ec, size_t bytes_transferred) {
      Cancel(receive_timeout_);
      if (ec) {
        if (boost::asio::error::eof != ec.value()) {
//          std::cerr << "receive failed: " << ec.message() << std::endl;
        }
        Close();
        OnError("receive failed");
        return;
      }
      std::cout << bytes_transferred << " byte(s) received." << std::endl;
      input_buffer_->consume(bytes_transferred);
      if (!OnReceive()) {
        Close();
        return;
      }
    });
  }

  bool SyncReceive() {
    Expire(receive_timeout_);
    boost::system::error_code ec;
    size_t bytes_transferred = socket_.read_some(boost::asio::buffer(
        input_buffer_->write_buffer(), input_buffer_->writable_bytes()), ec);
    if (ec) {
      if (boost::asio::error::eof != ec.value()) {
        std::cerr << "receive failed: " << ec.message() << std::endl;
      }
      Close();
      return false;
    }
    input_buffer_->consume(bytes_transferred);
    Cancel(receive_timeout_);
    return true;
  }

  void Receive() {
    future_ = socket_.async_read_some(boost::asio::buffer(input_buffer_->write_buffer(),
        input_buffer_->writable_bytes()), boost::asio::use_future);
    if (receive_timeout_ > std::chrono::milliseconds::zero()) {
      last_time_ = now();
    }
  }

  bool WaitForReceive() {
    std::size_t bytes_transferred = FutureWait("receive", receive_timeout_);
    if (!bytes_transferred) {
      return false;
    }
    input_buffer_->consume(bytes_transferred);
    return true;
  }

  void Close() {
    if (socket_.is_open()) {
      std::cout << "close socket, local(" <<
          local_.address() << ":" <<
          local_.port() << "), remote(" <<
          remote_.address() << ":" <<
          remote_.port() << ")." << std::endl;
      try {
        socket_.close();
      } catch (const boost::system::system_error& se) {
        std::cerr << "close failed: " << se.what() << std::endl;
      }
    }
    OnClose();
  }

  void Cancel() {
    try {
      socket_.cancel();
    } catch (const boost::system::system_error& se) {
      std::cerr << "cancel failed: " << se.what() << std::endl;
      Close();
    }
  }

  void io_service(boost::asio::io_service& io_service) {
    io_service_ = io_service;
  }
  boost::asio::io_service& io_service() {
    return socket_.get_io_service();
  }

  boost::asio::ip::tcp::socket& socket() {
    return socket_;
  }

  BufferPtr& input_buffer() {
    return input_buffer_;
  }

  const std::string& error() const {
    return error_;
  }

  void timeout(const boost::posix_time::millisec& connect_timeout,
      const boost::posix_time::millisec& send_timeout,
      const boost::posix_time::millisec& receive_timeout) {
    connect_timeout_ = connect_timeout;
    send_timeout_ = send_timeout;
    receive_timeout_ = receive_timeout;
  }

 protected:
  static std::chrono::time_point<std::chrono::steady_clock> now() {
    return std::chrono::steady_clock::now();
  }

  // for async service
  virtual bool OnConnect() { return true; }
  virtual bool OnSend() { return true; }
  virtual bool OnReceive() { return true; }
  virtual bool OnClose() { return true; }
  virtual void OnError(const std::string&) { return; }

  void* server_;

 private:
  TCPConnection(const TCPConnection&) = delete;
  TCPConnection& operator=(const TCPConnection&) = delete;

  void Expire(const std::chrono::milliseconds& timeout) {
    if (timeout > std::chrono::milliseconds::zero()) {
//      auto self(this->shared_from_this());
      timer_.expires_from_now(timeout);
//      timer_.async_wait([this, self](const boost::system::error_code& ec) {
      timer_.async_wait([this](const boost::system::error_code& ec) {
        if (timer_lock_.test_and_set(std::memory_order_acquire)) {
          return;
        }
//        if (ec == boost::asio::error::operation_aborted) {
//          return;
//        }
        Cancel();
      });
    }
  }

  void Cancel(const std::chrono::milliseconds& timeout) {
    if (timeout > std::chrono::milliseconds::zero()) {
      if (timer_lock_.test_and_set()) {
        return;
      }
      timer_.cancel();
    }
  }

  bool ConnectFutureWait() {
    if (connect_timeout_ > std::chrono::milliseconds::zero()) {
      auto current_time(now());
      auto timeout = std::max(connect_timeout_ -
          std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_time_),
          std::chrono::milliseconds::zero());
      std::future_status status = connect_future_.wait_for(timeout);
      if (status == std::future_status::timeout) {
        std::cerr << "connect failed: timeout" << std::endl;
        Cancel();
        return false;
      }
    } else {
      connect_future_.wait();
    }
    try {
      connect_future_.get();
    } catch (const boost::system::system_error& se) {
      std::cerr << "connect failed, local(" <<
          socket_.local_endpoint().address().to_string() << ":" <<
          socket_.local_endpoint().port() << "), remote(" <<
          remote_.address().to_string() << ":" <<
          remote_.port() << "): " << se.what() << std::endl;
      Close();
      return false;
    }
    local_ = socket_.local_endpoint();
    return true;
  }

  std::size_t FutureWait(const std::string& prefix,
      const std::chrono::milliseconds& wait_timeout) {
    if (wait_timeout > std::chrono::milliseconds::zero()) {
      auto current_time(now());
      auto timeout = std::max(wait_timeout -
          std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_time_),
          std::chrono::milliseconds::zero());
      std::future_status status = future_.wait_for(timeout);
      if (status == std::future_status::timeout) {
        std::cerr << prefix << " failed: timeout" << std::endl;
        Cancel();
        return 0;
      }
    } else {
      future_.wait();
    }
    std::size_t bytes_transferred(0);
    try {
      bytes_transferred = future_.get();
    } catch (const boost::system::system_error& se) {
      std::cerr << prefix << " failed: " << se.what() << std::endl;
      Close();
      return 0;
    }
    return bytes_transferred;
  }

  std::reference_wrapper<boost::asio::io_service> io_service_;
  boost::asio::ip::tcp::socket socket_;
  boost::asio::ip::tcp::endpoint local_, remote_;
  BufferPtr input_buffer_ { std::make_shared<InputBuffer>() };
  std::string error_;
  std::chrono::milliseconds connect_timeout_ { 10 }, send_timeout_ { 10 }, receive_timeout_ { 10 };
  SteadyTimer timer_;
  std::atomic_flag timer_lock_ { ATOMIC_FLAG_INIT };
  // for future connect, send and receive
  std::future<void> connect_future_;
  std::future<std::size_t> future_;
  BufferWeakPtr output_buffer_;
  std::chrono::time_point<std::chrono::steady_clock> last_time_;
};

}
