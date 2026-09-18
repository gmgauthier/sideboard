/* SPDX-License-Identifier: Unlicense */

#pragma once

#include "remote.hpp"

#include <gtkmm.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace sideboard {

class AppRow;

class Window : public Gtk::Window {
 public:
  Window();
  ~Window() override;

 private:
  void load_css();
  void build_menu();
  void build_body();
  void fill_rows();
  void apply_cache();
  void on_about();
  void on_quit();
  void on_refresh();
  void run_refresh();
  void on_refresh_progress();
  void on_refresh_done();
  void on_install(AppRow* row);
  void on_uninstall(AppRow* row);
  void run_download();
  void on_download_done();
  void set_busy(bool on);
  void stop_refresh();
  void size_to_list();
  void show_error(const Glib::ustring& msg);
  bool pkexec_helper(const char* verb, const std::string& arg, std::string& error);

  Gtk::Box root_{Gtk::ORIENTATION_VERTICAL, 0};
  Gtk::MenuBar menubar_;
  Gtk::EventBox banner_;
  Gtk::Box banner_box_{Gtk::ORIENTATION_VERTICAL, 2};
  Gtk::Label banner_title_;
  Gtk::Label banner_caption_;
  Gtk::Box cols_{Gtk::ORIENTATION_HORIZONTAL, 8};
  Gtk::Label* avail_head_ = nullptr;
  Gtk::ScrolledWindow scroll_;
  Gtk::ListBox list_;
  Gtk::Box bottom_{Gtk::ORIENTATION_HORIZONTAL, 8};
  Gtk::Label last_checked_;
  Gtk::Button refresh_{"Refresh"};
  Gtk::Button install_all_{"Install all updates"};
  Gtk::Label status_;
  Gtk::ProgressBar progress_;
  Gtk::MenuItem* refresh_item_ = nullptr;
  Gtk::MenuItem* install_all_item_ = nullptr;

  std::vector<AppRow*> rows_;
  std::thread refresh_thread_;
  std::mutex mu_;
  std::vector<Remote> refresh_result_;
  std::string progress_text_;
  double progress_frac_ = 0;
  std::string refresh_banner_;
  std::atomic<bool> cancel_{false};
  bool busy_ = false;
  AppRow* install_row_ = nullptr;
  Remote install_remote_;
  std::string install_dest_;
  std::string install_error_;
  bool install_ok_ = false;
  Glib::Dispatcher progress_dispatch_;
  Glib::Dispatcher done_dispatch_;
  sigc::connection progress_conn_;
  sigc::connection done_conn_;
};

}  // namespace sideboard
