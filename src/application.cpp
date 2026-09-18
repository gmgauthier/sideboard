/* SPDX-License-Identifier: Unlicense */

#include "application.hpp"
#include "window.hpp"
#include "config.hpp"

#include <fcntl.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <glib.h>
#include <glibmm/miscutils.h>

#include <cstring>
#include <iostream>

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

std::string runtime_dir()
{
  const std::string dir = Glib::get_user_runtime_dir();
  g_mkdir_with_parents(dir.c_str(), 0700);
  return dir;
}

std::string lock_path()
{
  return Glib::build_filename(runtime_dir(), "sideboard.lock");
}

std::string socket_path()
{
  return Glib::build_filename(runtime_dir(), "sideboard.sock");
}

}  // namespace

Glib::RefPtr<Application> Application::create()
{
  return Glib::RefPtr<Application>(new Application());
}

Application::Application()
    : Gtk::Application(APP_ID, Gio::APPLICATION_NON_UNIQUE)
{
}

Application::~Application()
{
  close_socket();
  if (lock_fd_ >= 0) {
    close(lock_fd_);
    lock_fd_ = -1;
  }
}

bool Application::take_instance_lock()
{
  lock_fd_ = ::open(lock_path().c_str(), O_CREAT | O_RDWR, 0600);
  if (lock_fd_ < 0)
    return true;
  if (flock(lock_fd_, LOCK_EX | LOCK_NB) != 0) {
    close(lock_fd_);
    lock_fd_ = -1;
    return false;
  }
  return true;
}

void Application::listen_socket()
{
  const std::string path = socket_path();
  ::unlink(path.c_str());
  listen_fd_ = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (listen_fd_ < 0)
    return;
  sockaddr_un addr;
  if (!fill_unix_addr(&addr, path) ||
      ::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
      ::listen(listen_fd_, 4) != 0) {
    close(listen_fd_);
    listen_fd_ = -1;
    return;
  }
  ::chmod(path.c_str(), 0600);
  listen_conn_ = Glib::signal_io().connect(sigc::mem_fun(*this, &Application::on_listen_io),
                                           listen_fd_, Glib::IO_IN | Glib::IO_HUP);
}

void Application::close_socket()
{
  listen_conn_.disconnect();
  if (listen_fd_ >= 0) {
    close(listen_fd_);
    listen_fd_ = -1;
  }
  ::unlink(socket_path().c_str());
}

bool Application::send_present() const
{
  sockaddr_un addr;
  if (!fill_unix_addr(&addr, socket_path()))
    return false;
  const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd < 0)
    return false;
  if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    close(fd);
    return false;
  }
  const char msg[] = "present\n";
  const ssize_t wrote = ::write(fd, msg, sizeof(msg) - 1);
  (void)wrote;
  close(fd);
  return true;
}

bool Application::on_listen_io(Glib::IOCondition)
{
  if (listen_fd_ < 0)
    return false;
  const int cfd = ::accept4(listen_fd_, nullptr, nullptr, SOCK_CLOEXEC);
  if (cfd < 0)
    return true;
  char buf[64];
  const ssize_t n = ::read(cfd, buf, sizeof(buf));
  (void)n;
  close(cfd);
  ensure_window();
  if (window_)
    window_->present();
  return true;
}

void Application::ensure_window()
{
  if (window_)
    return;
  auto* win = new Window();
  window_ = win;
  add_window(*win);
  win->signal_hide().connect([this, win]() {
    if (window_ == win)
      window_ = nullptr;
    delete win;
  });
}

void Application::on_startup()
{
  Gtk::Application::on_startup();
  if (auto settings = Gtk::Settings::get_default())
    settings->property_gtk_application_prefer_dark_theme() = false;
  lock_ok_ = take_instance_lock();
  if (lock_ok_)
    listen_socket();
}

void Application::on_activate()
{
  if (!lock_ok_) {
    if (!send_present())
      std::cerr << "sideboard: already running\n";
    quit();
    return;
  }
  ensure_window();
  if (window_)
    window_->present();
}

}  // namespace sideboard
