/* SPDX-License-Identifier: Unlicense */

#include "instance.hpp"

#include <fcntl.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstring>
#include <utility>

namespace sideboard {
namespace {

bool fill_unix_addr(sockaddr_un* addr, const std::string& path)
{
  if (path.size() >= sizeof(addr->sun_path))
    return false;
  std::memset(addr, 0, sizeof(*addr));
  addr->sun_family = AF_UNIX;
  std::memcpy(addr->sun_path, path.c_str(), path.size() + 1);
  return true;
}

}  // namespace

InstanceGuard::InstanceGuard(std::string dir)
    : dir_(std::move(dir))
{
}

InstanceGuard::~InstanceGuard()
{
  close();
  if (lock_fd_ >= 0) {
    ::close(lock_fd_);
    lock_fd_ = -1;
  }
}

std::string InstanceGuard::lock_path() const
{
  return dir_ + "/sideboard.lock";
}

std::string InstanceGuard::socket_path() const
{
  return dir_ + "/sideboard.sock";
}

bool InstanceGuard::take_lock()
{
  lock_fd_ = ::open(lock_path().c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
  if (lock_fd_ < 0)
    return true;
  if (flock(lock_fd_, LOCK_EX | LOCK_NB) != 0) {
    ::close(lock_fd_);
    lock_fd_ = -1;
    return false;
  }
  return true;
}

bool InstanceGuard::listen()
{
  const std::string path = socket_path();
  ::unlink(path.c_str());
  listen_fd_ = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (listen_fd_ < 0)
    return false;
  sockaddr_un addr;
  if (!fill_unix_addr(&addr, path) ||
      ::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
      ::listen(listen_fd_, 4) != 0) {
    ::close(listen_fd_);
    listen_fd_ = -1;
    return false;
  }
  ::chmod(path.c_str(), 0600);
  return true;
}

void InstanceGuard::close()
{
  /* Only the process that bound the socket removes it. A second launch never did. */
  if (listen_fd_ < 0)
    return;
  ::close(listen_fd_);
  listen_fd_ = -1;
  ::unlink(socket_path().c_str());
}

bool InstanceGuard::send_present() const
{
  sockaddr_un addr;
  if (!fill_unix_addr(&addr, socket_path()))
    return false;
  const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd < 0)
    return false;
  if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    ::close(fd);
    return false;
  }
  const char msg[] = "present\n";
  const ssize_t wrote = ::write(fd, msg, sizeof(msg) - 1);
  (void)wrote;
  ::close(fd);
  return true;
}

}  // namespace sideboard
