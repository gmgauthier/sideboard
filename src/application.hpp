/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <gtkmm.h>

namespace sideboard {

class Window;

class Application : public Gtk::Application {
 public:
  static Glib::RefPtr<Application> create();
  ~Application() override;

 protected:
  Application();
  void on_startup() override;
  void on_activate() override;

 private:
  bool take_instance_lock();
  void listen_socket();
  void close_socket();
  bool send_present() const;
  bool on_listen_io(Glib::IOCondition cond);
  void ensure_window();

  int lock_fd_ = -1;
  int listen_fd_ = -1;
  bool lock_ok_ = true;
  sigc::connection listen_conn_;
  Window* window_ = nullptr;
};

}  // namespace sideboard
