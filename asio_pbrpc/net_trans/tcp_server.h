// Copyright 2015, Xiaojie Chen (swly@live.com). All rights reserved.
// https://github.com/vorfeed/json
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

#pragma once

#include <string>
#include <iostream>
#include <vector>
#include <boost/asio.hpp>

#include "executors.h"
#include "tcp_connection.h"

namespace asio_pbrpc {

template <class Connection>
class TCPServer {
 public:
  static_assert(std::is_base_of<TCPConnection<typename
      Connection::BufferPtr::element_type>, Connection>::value, "");

  typedef typename Connection::Ptr ConnectionPtr;

  TCPServer(const boost::asio::ip::tcp::endpoint& server,
      const std::string& name = "") :
    name_(name), acceptor_(listening_executor_.io_service(), server) {}

  TCPServer(const std::string& host, int port, const std::string& name = "") :
    TCPServer(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(host), port), name) {}

  TCPServer(int port, const std::string& name = "") :
    TCPServer("127.0.0.1", port, name) {}

  void Start() {
    listening_executor_.Start();
    conenection_executor_.Start(4);
    working_executor_.Start(4, std::max(std::thread::hardware_concurrency() / 4, 1u));
    StartAccept();
  }

  void Stop() {
    working_executor_.Stop();
    conenection_executor_.Stop();
    listening_executor_.Stop();
  }

  const std::string& name() const {
    return name_;
  }

 protected:
  Executor conenection_executor_;
  Executors working_executor_;

 private:
  TCPServer(const TCPServer&) = delete;
  TCPServer& operator=(const TCPServer&) = delete;

  void StartAccept() {
    ConnectionPtr connection(std::make_shared<Connection>(
        listening_executor_.io_service(), this));
    acceptor_.async_accept(connection->socket(),
        [this, connection](const boost::system::error_code& ec) {
      if (ec) {
        std::cerr << "accept failed: " << ec.message() << std::endl;
        return;
      }
      if (connection->Start()) {
        connection->AsyncReceive();
      }
      StartAccept();
    });
  }

  const std::string name_;
  Executor listening_executor_;
  boost::asio::ip::tcp::acceptor acceptor_;
};

}

