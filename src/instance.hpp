/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <string>

namespace sideboard {

/* Single-instance lock and the "present" socket, kept in one runtime directory. */
class InstanceGuard {
 public:
  explicit InstanceGuard(std::string dir);
  ~InstanceGuard();
  InstanceGuard(const InstanceGuard&) = delete;
  InstanceGuard& operator=(const InstanceGuard&) = delete;

  bool take_lock();
  bool listen();
  void close();
  bool send_present() const;
  int listen_fd() const
  {
    return listen_fd_;
  }
  std::string lock_path() const;
  std::string socket_path() const;

 private:
  std::string dir_;
  int lock_fd_ = -1;
  int listen_fd_ = -1;
};

}  // namespace sideboard
