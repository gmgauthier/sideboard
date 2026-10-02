/* SPDX-License-Identifier: Unlicense */

#include "application.hpp"
#include "window.hpp"
#include "config.hpp"
#include "instance.hpp"

#include <sys/socket.h>
#include <unistd.h>

#include <glib.h>
#include <glibmm/miscutils.h>

#include <iostream>

namespace sideboard {
namespace {

std::string runtime_dir()
{
  const std::string dir = Glib::get_user_runtime_dir();
  g_mkdir_with_parents(dir.c_str(), 0700);
  return dir;
}

}  // namespace

Glib::RefPtr<Application> Application::create()
{
  return Glib::RefPtr<Application>(new Application());
}

Application::Application()
    : Gtk::Application(APP_ID, Gio::APPLICATION_NON_UNIQUE),
      instance_(runtime_dir())
{
}

Application::~Application()
{
  close_socket();
}

bool Application::take_instance_lock()
{
  return instance_.take_lock();
}

void Application::listen_socket()
{
  if (!instance_.listen())
    return;
  listen_conn_ = Glib::signal_io().connect(sigc::mem_fun(*this, &Application::on_listen_io),
                                           instance_.listen_fd(), Glib::IO_IN | Glib::IO_HUP);
}

void Application::close_socket()
{
  listen_conn_.disconnect();
  instance_.close();
}

bool Application::send_present() const
{
  return instance_.send_present();
}

bool Application::on_listen_io(Glib::IOCondition)
{
  const int listen_fd = instance_.listen_fd();
  if (listen_fd < 0)
    return false;
  const int cfd = ::accept4(listen_fd, nullptr, nullptr, SOCK_CLOEXEC);
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
