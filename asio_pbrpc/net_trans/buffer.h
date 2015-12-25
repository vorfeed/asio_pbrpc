// Copyright 2015, Xiaojie Chen (swly@live.com). All rights reserved.
// https://github.com/vorfeed/json
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

#pragma once

#include <cassert>
#include <cstring>
#include <algorithm>
#include <memory>
#include <vector>

namespace asio_pbrpc {

class Buffer;

typedef std::shared_ptr<Buffer> BufferPtr;
typedef std::weak_ptr<Buffer> BufferWeakPtr;

class Buffer {
 public:
  Buffer(size_t size = kInitSize) : buffer_(kInitSize) {}

  const char* read_buffer() const {
    return begin() + read_index_;
  }
  const char* write_buffer() const {
    return begin() + write_index_;
  }
  char* write_buffer() {
    return begin() + write_index_;
  }

  /// Append, then consume.
  void consume(size_t len) {
    write_index_ += len;
  }

  void retrieve(size_t len) {
    assert(len <= readable_bytes());
    if (len < readable_bytes()) {
      read_index_ += len;
    } else {
      retrieve();
    }
  }
  void retrieve() {
    read_index_ = write_index_ = 0;
  }

  template <typename Type>
  bool readable() {
    return sizeof(Type) <= readable_bytes();
  }
  bool readable(size_t len) {
    return len <= readable_bytes();
  }

  template <typename Type>
  Type read() {
    Type ret(*reinterpret_cast<const Type*>(read_buffer()));
    retrieve(sizeof(Type));
    return ret;
  }
  const char* read(size_t len) {
    const char* ret = read_buffer();
    retrieve(len);
    return ret;
  }

  template <typename Type>
  void write(Type value) {
    ensure_writable_bytes(sizeof(Type));
    *reinterpret_cast<Type*>(write_buffer()) = value;
    consume(sizeof(Type));
  }
  void write(const char* data, size_t len) {
    ensure_writable_bytes(len);
    std::copy(data, data + len, write_buffer());
    consume(len);
  }

  size_t readable_bytes() const {
    return write_index_ - read_index_;
  }
  size_t writable_bytes() const {
    return buffer_.size() - write_index_;
  }

  size_t capacity() const {
    return buffer_.capacity();
  }

  void shrink(size_t reserve) {
    Buffer other;
    other.ensure_writable_bytes(readable_bytes() + reserve);
    other.write(read_buffer(), readable_bytes());
    swap(other);
  }

  void expand(size_t len) {
    buffer_.resize(buffer_.size() + len);
  }

  void swap(Buffer& rhs) {
    buffer_.swap(rhs.buffer_);
    std::swap(read_index_, rhs.read_index_);
    std::swap(write_index_, rhs.write_index_);
  }

 protected:
  char* begin() {
    return &*buffer_.begin();
  }
  const char* begin() const {
    return &*buffer_.begin();
  }

  void make_space(size_t len) {
    if (writable_bytes() < len) {
      buffer_.resize(write_index_ + len);
    } else {
      std::copy(begin() + read_index_, begin() + write_index_, begin());
      write_index_ -= read_index_;
      read_index_ = 0;
    }
  }

  void ensure_writable_bytes(size_t len) {
    if (writable_bytes() < len) {
      make_space(len);
    }
    assert(writable_bytes() >= len);
  }

 private:
  static const size_t kInitSize = 1024;
  std::vector<char> buffer_;
  size_t read_index_ { 0 }, write_index_ { 0 };
};

}
