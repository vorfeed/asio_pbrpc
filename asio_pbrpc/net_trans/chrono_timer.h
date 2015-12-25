// Copyright 2015, Xiaojie Chen (swly@live.com). All rights reserved.
// https://github.com/vorfeed/json
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

#pragma once

#include <chrono>

#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

template <typename Clock>
struct ChronoTimerService {
  typedef typename Clock::time_point time_type;
  typedef typename Clock::duration duration_type;

  static time_type now() {
    return Clock::now();
  }

  static time_type add(time_type t, duration_type d) {
    return t + d;
  }

  static duration_type subtract(time_type t1, time_type t2) {
    return t1 - t2;
  }

  static bool less_than(time_type t1, time_type t2) {
    return t1 < t2;
  }

  static boost::posix_time::time_duration to_posix_duration(duration_type d) {
    auto in_sec = std::chrono::duration_cast<std::chrono::seconds>(d);
    auto in_usec = std::chrono::duration_cast<std::chrono::microseconds>(d - in_sec);
    return boost::posix_time::seconds(in_sec.count()) +
        boost::posix_time::microseconds(in_usec.count());
  }
};

typedef boost::asio::basic_deadline_timer<std::chrono::system_clock,
    ChronoTimerService<std::chrono::system_clock>> SystemTimer;
typedef boost::asio::basic_deadline_timer<std::chrono::steady_clock,
    ChronoTimerService<std::chrono::steady_clock>> SteadyTimer;
