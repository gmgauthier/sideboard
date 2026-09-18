/* SPDX-License-Identifier: Unlicense */

#include "window.hpp"
#include "about_dialog.hpp"
#include "cache.hpp"
#include "catalog.hpp"
#include "compare.hpp"
#include "config.hpp"
#include "fetch.hpp"
#include "local.hpp"
#include "paths.hpp"

#include <glibmm.h>

#include <algorithm>
#include <cstring>
#include <sys/wait.h>

namespace sideboard {
namespace {

const int kColApp = 150;
const int kColVer = 88;
const int kColStatus = 140;
const int kColAction = 108;
const int kColRemove = 40;

void size_label(Gtk::Label& lab, int width, float xalign)
{
  lab.set_xalign(xalign);
  lab.set_ellipsize(Pango::ELLIPSIZE_END);
  lab.set_size_request(width, -1);
}

Gtk::Label* col_header(const char* text, int width, float xalign)
{
  auto* lab = Gtk::manage(new Gtk::Label(text));
  lab->set_xalign(xalign);
  lab->get_style_context()->add_class("sideboard-colhead");
  lab->set_size_request(width, -1);
  return lab;
}

void set_status_class(Gtk::Label& lab, const char* klass)
{
  auto ctx = lab.get_style_context();
  ctx->remove_class("sideboard-status-update");
  ctx->remove_class("sideboard-status-ok");
  ctx->remove_class("sideboard-status-error");
  if (klass && klass[0])
    ctx->add_class(klass);
}

}  // namespace

class AppRow : public Gtk::ListBoxRow {
 public:
  explicit AppRow(const App& app)
      : package_(app.package),
        display_(app.display)
  {
    get_style_context()->add_class("sideboard-row");
    box_.set_spacing(8);
    box_.set_border_width(4);

    name_.set_text(app.display);
    name_.set_xalign(0);
    size_label(name_, kColApp, 0);
    size_label(installed_, kColVer, 0.5);
    size_label(available_, kColVer, 0.5);
    size_label(status_, kColStatus, 0);
    action_.set_sensitive(false);
    action_.set_size_request(kColAction, -1);
    action_.get_style_context()->add_class("sideboard-action");
    action_.signal_clicked().connect([this]() { signal_install_.emit(); });

    auto theme = Gtk::IconTheme::get_default();
    const char* icon = (theme && theme->has_icon("user-trash")) ? "user-trash" : "edit-delete";
    remove_img_.set_from_icon_name(icon, Gtk::ICON_SIZE_MENU);
    remove_.set_image(remove_img_);
    remove_.set_always_show_image(true);
    remove_.set_tooltip_text("Uninstall");
    remove_.get_style_context()->add_class("sideboard-remove");
    remove_.set_sensitive(false);
    remove_.set_size_request(kColRemove, -1);
    remove_.signal_clicked().connect([this]() { signal_remove_.emit(); });

    box_.pack_start(name_, Gtk::PACK_SHRINK);
    box_.pack_start(installed_, Gtk::PACK_SHRINK);
    box_.pack_start(available_, Gtk::PACK_SHRINK);
    box_.pack_start(status_, Gtk::PACK_EXPAND_WIDGET);
    box_.pack_start(action_, Gtk::PACK_SHRINK);
    box_.pack_start(remove_, Gtk::PACK_SHRINK);
    add(box_);
    apply(query_installed(app.package), nullptr);
    show_all();
  }

  const char* package() const
  {
    return package_;
  }

  const char* display() const
  {
    return display_;
  }

  const Remote& remote() const
  {
    return remote_;
  }

  bool wants_upgrade() const
  {
    if (!remote_.ok || remote_.url.empty())
      return false;
    const Installed inst = query_installed(package_);
    return inst.present && version_older(inst.debian, remote_.debian);
  }

  sigc::signal<void>& signal_install()
  {
    return signal_install_;
  }

  sigc::signal<void>& signal_remove()
  {
    return signal_remove_;
  }

  void apply(const Installed& inst, const Remote* remote)
  {
    if (remote)
      remote_ = *remote;
    else
      remote_ = Remote{};

    installed_.set_text(inst.present ? Glib::ustring(inst.upstream) : Glib::ustring("—"));
    set_status_class(status_, nullptr);
    action_.set_sensitive(false);
    const bool self = package_ && std::strcmp(package_, "sideboard") == 0;
    remove_.set_sensitive(inst.present && !self);

    if (remote_.no_release) {
      available_.set_text("—");
      action_.set_sensitive(false);
      action_.set_label("—");
      status_.set_text("No release");
      return;
    }
    if (!remote_.ok && remote_.error.empty() && remote_.url.empty()) {
      available_.set_text("—");
      action_.set_sensitive(false);
      if (inst.present) {
        status_.set_text("—");
        action_.set_label("—");
      } else {
        status_.set_text("Not installed");
        action_.set_label("Install");
      }
      return;
    }
    if (!remote_.ok) {
      available_.set_text(remote_.upstream.empty() ? "—" : remote_.upstream);
      action_.set_sensitive(!remote_.url.empty());
      if (remote_.url.empty()) {
        action_.set_label("—");
        status_.set_text("Not available");
        return;
      }
      status_.set_text("Error");
      set_status_class(status_, "sideboard-status-error");
      action_.set_label(inst.present ? "Upgrade" : "Install");
      return;
    }
    available_.set_text(remote_.upstream);
    if (!inst.present) {
      status_.set_text("Not installed");
      action_.set_label("Install");
      action_.set_sensitive(!remote_.url.empty());
      return;
    }
    if (version_older(inst.debian, remote_.debian)) {
      status_.set_text("Update available");
      set_status_class(status_, "sideboard-status-update");
      action_.set_label("Upgrade");
      action_.set_sensitive(!remote_.url.empty());
      return;
    }
    status_.set_text("Up to date");
    set_status_class(status_, "sideboard-status-ok");
    action_.set_label("—");
  }

  void set_checking()
  {
    set_status_class(status_, nullptr);
    status_.set_text("Checking…");
    action_.set_sensitive(false);
    remove_.set_sensitive(false);
  }

  void set_installing()
  {
    set_status_class(status_, nullptr);
    status_.set_text("Installing…");
    action_.set_sensitive(false);
    remove_.set_sensitive(false);
  }

  void set_removing()
  {
    set_status_class(status_, nullptr);
    status_.set_text("Removing…");
    action_.set_sensitive(false);
    remove_.set_sensitive(false);
  }

  void set_locked(bool on)
  {
    if (on) {
      action_.set_sensitive(false);
      remove_.set_sensitive(false);
    } else {
      apply(query_installed(package_), remote_.ok || !remote_.url.empty() ? &remote_ : nullptr);
    }
  }

 private:
  const char* package_ = nullptr;
  const char* display_ = nullptr;
  Remote remote_;
  sigc::signal<void> signal_install_;
  sigc::signal<void> signal_remove_;
  Gtk::Image remove_img_;
  Gtk::Button remove_;
  Gtk::Box box_{Gtk::ORIENTATION_HORIZONTAL, 8};
  Gtk::Label name_;
  Gtk::Label installed_;
  Gtk::Label available_;
  Gtk::Label status_;
  Gtk::Button action_;
};

Window::Window()
{
  set_title("Sideboard");
  set_resizable(true);
  set_default_size(760, -1);
  get_style_context()->add_class("sideboard-window");
  load_css();
  progress_conn_ = progress_dispatch_.connect(sigc::mem_fun(*this, &Window::on_refresh_progress));
  done_conn_ = done_dispatch_.connect(sigc::mem_fun(*this, &Window::on_refresh_done));
  build_menu();
  build_body();
  add(root_);
  fill_rows();
  apply_cache();
  show_all();
  progress_.hide();
  size_to_list();
}

Window::~Window()
{
  progress_conn_.disconnect();
  done_conn_.disconnect();
  stop_refresh();
}

void Window::load_css()
{
  const std::string path = find_data_file("sideboard.css");
  if (path.empty())
    return;
  auto css = Gtk::CssProvider::create();
  try {
    css->load_from_path(path);
  } catch (const Glib::Error&) {
    return;
  }
  auto screen = Gdk::Screen::get_default();
  if (!screen)
    return;
  Gtk::StyleContext::add_provider_for_screen(screen, css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

void Window::build_menu()
{
  auto* file = Gtk::manage(new Gtk::Menu());
  refresh_item_ = Gtk::manage(new Gtk::MenuItem("_Refresh", true));
  refresh_item_->signal_activate().connect(sigc::mem_fun(*this, &Window::on_refresh));
  file->append(*refresh_item_);
  install_all_item_ = Gtk::manage(new Gtk::MenuItem("Install all _updates", true));
  install_all_item_->signal_activate().connect(sigc::mem_fun(*this, &Window::on_install_all));
  file->append(*install_all_item_);
  file->append(*Gtk::manage(new Gtk::SeparatorMenuItem()));
  auto* quit = Gtk::manage(new Gtk::MenuItem("_Quit", true));
  quit->signal_activate().connect(sigc::mem_fun(*this, &Window::on_quit));
  file->append(*quit);
  auto* file_item = Gtk::manage(new Gtk::MenuItem("_File", true));
  file_item->set_submenu(*file);
  menubar_.append(*file_item);

  auto* help = Gtk::manage(new Gtk::Menu());
  auto* about = Gtk::manage(new Gtk::MenuItem("_About Sideboard", true));
  about->signal_activate().connect(sigc::mem_fun(*this, &Window::on_about));
  help->append(*about);
  auto* help_item = Gtk::manage(new Gtk::MenuItem("_Help", true));
  help_item->set_submenu(*help);
  menubar_.append(*help_item);

  root_.pack_start(menubar_, Gtk::PACK_SHRINK);
}

void Window::build_body()
{
  banner_.get_style_context()->add_class("sideboard-banner");
  banner_title_.set_text("Retro Software For LCOS");
  banner_title_.set_xalign(0);
  banner_title_.get_style_context()->add_class("sideboard-banner-title");
  banner_caption_.set_text("Lovingly crafted for LCOS by Grok (@gmgauthier on GitHub)");
  banner_caption_.set_xalign(0);
  banner_caption_.set_line_wrap(true);
  banner_caption_.get_style_context()->add_class("sideboard-banner-caption");
  banner_box_.set_border_width(10);
  banner_box_.pack_start(banner_title_, Gtk::PACK_SHRINK);
  banner_box_.pack_start(banner_caption_, Gtk::PACK_SHRINK);
  banner_.add(banner_box_);
  root_.pack_start(banner_, Gtk::PACK_SHRINK);

  cols_.set_border_width(8);
  cols_.set_margin_bottom(0);
  cols_.pack_start(*col_header("App", kColApp, 0), Gtk::PACK_SHRINK);
  cols_.pack_start(*col_header("Installed", kColVer, 0.5), Gtk::PACK_SHRINK);
  avail_head_ = col_header("Available", kColVer, 0.5);
  cols_.pack_start(*avail_head_, Gtk::PACK_SHRINK);
  cols_.pack_start(*col_header("Status", kColStatus, 0), Gtk::PACK_EXPAND_WIDGET);
  cols_.pack_start(*col_header("", kColAction, 0.5), Gtk::PACK_SHRINK);
  cols_.pack_start(*col_header("", kColRemove, 0.5), Gtk::PACK_SHRINK);
  root_.pack_start(cols_, Gtk::PACK_SHRINK);

  list_.set_selection_mode(Gtk::SELECTION_NONE);
  list_.get_style_context()->add_class("sideboard-list");
  scroll_.add(list_);
  scroll_.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_NEVER);
  scroll_.set_propagate_natural_height(true);
  scroll_.set_shadow_type(Gtk::SHADOW_IN);
  scroll_.set_hexpand(true);
  scroll_.set_vexpand(false);
  scroll_.set_margin_start(8);
  scroll_.set_margin_end(8);
  root_.pack_start(scroll_, Gtk::PACK_SHRINK);

  last_checked_.set_text("Last checked: —");
  last_checked_.set_xalign(0);
  last_checked_.set_hexpand(true);
  refresh_.signal_clicked().connect(sigc::mem_fun(*this, &Window::on_refresh));
  install_all_.signal_clicked().connect(sigc::mem_fun(*this, &Window::on_install_all));
  bottom_.set_border_width(8);
  bottom_.pack_start(last_checked_, Gtk::PACK_EXPAND_WIDGET);
  bottom_.pack_start(refresh_, Gtk::PACK_SHRINK);
  bottom_.pack_start(install_all_, Gtk::PACK_SHRINK);
  root_.pack_start(bottom_, Gtk::PACK_SHRINK);

  status_.set_xalign(0);
  status_.set_margin_start(8);
  status_.set_margin_end(8);
  progress_.set_margin_start(8);
  progress_.set_margin_end(8);
  progress_.set_margin_bottom(8);
  progress_.set_no_show_all();
  root_.pack_start(status_, Gtk::PACK_SHRINK);
  root_.pack_start(progress_, Gtk::PACK_SHRINK);
}

void Window::fill_rows()
{
  auto children = list_.get_children();
  for (auto* w : children)
    list_.remove(*w);
  rows_.clear();

  std::size_t n = 0;
  const App* apps = catalog(&n);
  for (std::size_t i = 0; i < n; ++i) {
    auto* row = Gtk::manage(new AppRow(apps[i]));
    row->signal_install().connect([this, row]() { on_install(row); });
    row->signal_remove().connect([this, row]() { on_uninstall(row); });
    list_.append(*row);
    rows_.push_back(row);
  }
  list_.show_all();
}

void Window::apply_cache()
{
  const CatalogCache cache = load_cache();
  const std::string stamp = cache.checked.empty() ? load_last_check() : cache.checked;
  if (cache.apps.empty()) {
    if (avail_head_)
      avail_head_->set_text("Available");
    last_checked_.set_text("Last checked: —");
    update_install_all();
    return;
  }
  if (avail_head_)
    avail_head_->set_text("Available (cached)");
  last_checked_.set_text(stamp.empty() ? "Last checked: — (cached)"
                                       : "Last checked: " + stamp + " (cached)");
  for (auto* row : rows_) {
    auto it = cache.apps.find(row->package());
    const Remote* rem = (it == cache.apps.end()) ? nullptr : &it->second;
    row->apply(query_installed(row->package()), rem);
  }
  update_install_all();
}

void Window::set_busy(bool on)
{
  busy_ = on;
  refresh_.set_sensitive(!on);
  if (refresh_item_)
    refresh_item_->set_sensitive(!on);
  for (auto* row : rows_)
    row->set_locked(on);
  if (on) {
    install_all_.set_sensitive(false);
    if (install_all_item_)
      install_all_item_->set_sensitive(false);
  } else {
    update_install_all();
  }
}

void Window::update_install_all()
{
  bool any = false;
  for (auto* row : rows_) {
    if (row->wants_upgrade()) {
      any = true;
      break;
    }
  }
  install_all_.set_sensitive(!busy_ && any);
  if (install_all_item_)
    install_all_item_->set_sensitive(!busy_ && any);
}

void Window::stop_refresh()
{
  cancel_ = true;
  if (refresh_thread_.joinable())
    refresh_thread_.join();
  busy_ = false;
}

void Window::on_refresh()
{
  if (busy_)
    return;
  cancel_ = false;
  set_busy(true);
  status_.set_text("Checking GitHub releases…");
  progress_.set_fraction(0);
  progress_.show();
  for (auto* row : rows_)
    row->set_checking();
  refresh_thread_ = std::thread([this]() { run_refresh(); });
}

void Window::run_refresh()
{
  std::size_t n = 0;
  const App* apps = catalog(&n);
  std::vector<Remote> results(n);
  int failed = 0;
  std::string banner;
  for (std::size_t i = 0; i < n; ++i) {
    if (cancel_)
      break;
    {
      std::lock_guard<std::mutex> lock(mu_);
      progress_text_ = std::string("Checking ") + apps[i].display + "…";
      progress_frac_ = (n == 0) ? 0 : static_cast<double>(i) / static_cast<double>(n);
    }
    progress_dispatch_.emit();
    results[i] = fetch_latest(apps[i].owner, apps[i].repo, apps[i].package);
    if (results[i].rate_limited) {
      for (std::size_t j = i + 1; j < n; ++j) {
        results[j].rate_limited = true;
        results[j].error = results[i].error;
      }
      break;
    }
    if (!results[i].ok && !results[i].no_release) {
      ++failed;
      if (banner.empty())
        banner = results[i].error;
    }
  }
  {
    std::lock_guard<std::mutex> lock(mu_);
    refresh_result_ = std::move(results);
    refresh_banner_ = banner;
    progress_frac_ = 1;
    if (cancel_)
      progress_text_ = "Check cancelled.";
    else if (failed == static_cast<int>(n))
      progress_text_ = banner.empty() ? "Check failed." : ("Check failed: " + banner);
    else if (failed > 0)
      progress_text_ = "Checked with errors.";
    else
      progress_text_ = "Checked GitHub releases.";
  }
  done_dispatch_.emit();
}

void Window::on_refresh_progress()
{
  std::lock_guard<std::mutex> lock(mu_);
  status_.set_text(progress_text_);
  progress_.set_fraction(progress_frac_);
}

void Window::on_refresh_done()
{
  if (install_row_) {
    on_download_done();
    return;
  }
  if (refresh_thread_.joinable())
    refresh_thread_.join();
  set_busy(false);
  progress_.hide();

  std::vector<Remote> results;
  std::string banner;
  {
    std::lock_guard<std::mutex> lock(mu_);
    results = refresh_result_;
    banner = progress_text_;
  }
  status_.set_text(banner);

  CatalogCache cache = load_cache();
  cache.checked = now_stamp();
  std::size_t n = 0;
  const App* apps = catalog(&n);
  int live_ok = 0;
  for (std::size_t i = 0; i < rows_.size() && i < n; ++i) {
    Remote rem = (i < results.size()) ? results[i] : Remote{};
    if (rem.ok) {
      rem.cached = false;
      cache.apps[apps[i].package] = rem;
      ++live_ok;
    } else if (!rem.no_release) {
      auto it = cache.apps.find(apps[i].package);
      if (it != cache.apps.end()) {
        rem = it->second;
        rem.cached = true;
        rem.ok = true;
      }
    }
    const bool show = rem.ok || rem.no_release || !rem.error.empty();
    rows_[i]->apply(query_installed(apps[i].package), show ? &rem : nullptr);
  }
  if (live_ok > 0) {
    save_cache(cache);
    save_last_check(cache.checked);
    last_checked_.set_text("Last checked: " + cache.checked);
    if (avail_head_)
      avail_head_->set_text("Available");
  }
  bool rate = false;
  for (const auto& rem : results) {
    if (rem.rate_limited) {
      rate = true;
      break;
    }
  }
  if (rate) {
    status_.set_text(cache.apps.empty() ? "GitHub rate limit. Try again later."
                                        : "GitHub rate limit. Showing cached versions.");
  }
  update_install_all();
}

void Window::show_error(const Glib::ustring& msg)
{
  Gtk::MessageDialog dlg(*this, "Failed", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
  dlg.set_secondary_text(msg);
  dlg.run();
}

std::string deb_basename(const std::string& url)
{
  auto slash = url.find_last_of('/');
  std::string name = (slash == std::string::npos) ? url : url.substr(slash + 1);
  const auto q = name.find('?');
  if (q != std::string::npos)
    name = name.substr(0, q);
  return name;
}

bool Window::pkexec_helper(const char* verb, const std::string& arg, std::string& error)
{
  error.clear();
  if (!Glib::file_test(HELPER_PATH, Glib::FILE_TEST_IS_EXECUTABLE)) {
    error = "sideboard-helper is not installed at " HELPER_PATH
            ".\nInstall it with: sudo ninja -C build install";
    return false;
  }
  std::vector<std::string> argv = {"pkexec", HELPER_PATH, verb, arg};
  std::string out;
  std::string err;
  int wait_status = 0;
  try {
    Glib::spawn_sync("", argv, Glib::SPAWN_SEARCH_PATH, Glib::SlotSpawnChildSetup(), &out, &err,
                     &wait_status);
  } catch (const Glib::Error& e) {
    error = e.what();
    return false;
  }
  if (out.compare(0, 3, "OK\n") == 0 || out == "OK")
    return true;
  std::string line = out;
  if (line.compare(0, 4, "ERR ") == 0)
    line = line.substr(4);
  while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
    line.pop_back();
  if (line.empty())
    line = err;
  while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
    line.pop_back();
  if (!WIFEXITED(wait_status) || WEXITSTATUS(wait_status) != 0) {
    if (line.empty())
      line = "Authentication cancelled or helper failed.";
    error = line;
    return false;
  }
  if (line.empty())
    return true;
  error = line;
  return false;
}

bool Window::begin_download(AppRow* row)
{
  if (!row)
    return false;
  const Remote rem = row->remote();
  if (!rem.ok || rem.url.empty()) {
    show_error("No download URL. Refresh first.");
    return false;
  }
  const std::string name = deb_basename(rem.url);
  const std::string suffix = "_amd64.deb";
  if (name.size() <= suffix.size() ||
      name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0) {
    show_error("Release asset is not a .deb.");
    return false;
  }
  install_row_ = row;
  install_remote_ = rem;
  install_dest_ = Glib::build_filename(debs_dir(), name);
  install_error_.clear();
  install_ok_ = false;
  cancel_ = false;
  row->set_installing();
  Glib::ustring msg = "Downloading " + name + "…";
  if (!batch_.empty())
    msg = Glib::ustring::compose("Updating %1 (%2/%3)…", row->display(),
                                 static_cast<unsigned>(batch_index_ + 1),
                                 static_cast<unsigned>(batch_.size()));
  status_.set_text(msg);
  progress_.set_fraction(0);
  progress_.show();
  refresh_thread_ = std::thread([this]() { run_download(); });
  return true;
}

void Window::on_install(AppRow* row)
{
  if (busy_ || !row)
    return;
  if (dpkg_architecture() != "amd64") {
    Gtk::MessageDialog dlg(*this, "This machine is not amd64.", false, Gtk::MESSAGE_ERROR,
                           Gtk::BUTTONS_OK, true);
    dlg.set_secondary_text("Sideboard only installs amd64 .deb packages.");
    dlg.run();
    return;
  }
  batch_.clear();
  batch_index_ = 0;
  set_busy(true);
  if (!begin_download(row))
    set_busy(false);
}

void Window::on_install_all()
{
  if (busy_)
    return;
  if (dpkg_architecture() != "amd64") {
    Gtk::MessageDialog dlg(*this, "This machine is not amd64.", false, Gtk::MESSAGE_ERROR,
                           Gtk::BUTTONS_OK, true);
    dlg.set_secondary_text("Sideboard only installs amd64 .deb packages.");
    dlg.run();
    return;
  }
  batch_.clear();
  for (auto* row : rows_) {
    if (row->wants_upgrade())
      batch_.push_back(row);
  }
  if (batch_.empty()) {
    status_.set_text("No updates.");
    return;
  }
  batch_index_ = 0;
  set_busy(true);
  if (!begin_download(batch_[0])) {
    batch_.clear();
    set_busy(false);
  }
}

void Window::run_download()
{
  std::string err;
  const bool ok =
      download_file(install_remote_.url, install_dest_, err, [this](long now, long total) {
        std::lock_guard<std::mutex> lock(mu_);
        progress_frac_ = (total > 0) ? static_cast<double>(now) / static_cast<double>(total) : 0;
        progress_text_ = "Downloading…";
        progress_dispatch_.emit();
      });
  {
    std::lock_guard<std::mutex> lock(mu_);
    install_ok_ = ok;
    install_error_ = err;
    progress_frac_ = 1;
    progress_text_ = ok ? "Verifying…" : err;
  }
  done_dispatch_.emit();
}

void Window::on_download_done()
{
  if (refresh_thread_.joinable())
    refresh_thread_.join();

  AppRow* row = install_row_;
  const Remote rem = install_remote_;
  const std::string dest = install_dest_;
  const bool dl_ok = install_ok_;
  const std::string dl_err = install_error_;
  install_row_ = nullptr;

  auto fail_batch = [&](const Glib::ustring& msg) {
    batch_.clear();
    batch_index_ = 0;
    progress_.hide();
    set_busy(false);
    row->apply(query_installed(row->package()), &rem);
    status_.set_text("Error");
    show_error(msg);
  };

  if (!dl_ok) {
    fail_batch(dl_err.empty() ? "Download failed." : dl_err);
    return;
  }

  if (!rem.digest.empty()) {
    std::string hex_err;
    const std::string hex = sha256_file(dest, hex_err);
    if (hex.empty() || !digest_matches(rem.digest, hex)) {
      fail_batch(hex.empty() ? hex_err : "SHA-256 mismatch. The file was not installed.");
      return;
    }
  }

  status_.set_text(batch_.empty() ? Glib::ustring("Installing…")
                                  : Glib::ustring::compose("Installing %1 (%2/%3)…", row->display(),
                                                           static_cast<unsigned>(batch_index_ + 1),
                                                           static_cast<unsigned>(batch_.size())));
  while (Gtk::Main::events_pending())
    Gtk::Main::iteration();

  std::string err;
  const bool ok = pkexec_helper("install", dest, err);
  row->apply(query_installed(row->package()), &rem);
  if (!ok) {
    fail_batch(err.empty() ? "Install failed." : err);
    return;
  }

  if (!batch_.empty()) {
    ++batch_index_;
    if (batch_index_ < batch_.size()) {
      if (!begin_download(batch_[batch_index_])) {
        batch_.clear();
        batch_index_ = 0;
        progress_.hide();
        set_busy(false);
      }
      return;
    }
    const std::size_t n = batch_.size();
    batch_.clear();
    batch_index_ = 0;
    progress_.hide();
    set_busy(false);
    status_.set_text(Glib::ustring::compose("Updated %1 app(s).", static_cast<unsigned>(n)));
    return;
  }

  progress_.hide();
  set_busy(false);
  status_.set_text("Installed " + Glib::ustring(row->package()) + ".");
}

void Window::size_to_list()
{
  int min_h = 0, nat_h = 0;
  get_preferred_height(min_h, nat_h);
  int w = 0, h = 0;
  get_size(w, h);
  if (nat_h > 0)
    resize(std::max(w, 760), nat_h);
}

void Window::on_uninstall(AppRow* row)
{
  if (busy_ || !row)
    return;
  Gtk::MessageDialog ask(*this, Glib::ustring("Uninstall ") + row->display() + "?", false,
                         Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, true);
  ask.set_secondary_text(Glib::ustring("This runs apt-get remove on ") + row->package() + ".");
  if (ask.run() != Gtk::RESPONSE_YES)
    return;

  set_busy(true);
  row->set_removing();
  status_.set_text(Glib::ustring("Removing ") + row->display() + "…");
  while (Gtk::Main::events_pending())
    Gtk::Main::iteration();

  std::string err;
  const bool ok = pkexec_helper("remove", row->package(), err);
  set_busy(false);
  const Remote rem = row->remote();
  row->apply(query_installed(row->package()), rem.ok || !rem.url.empty() ? &rem : nullptr);
  update_install_all();
  if (!ok) {
    status_.set_text("Error");
    show_error(err.empty() ? "Remove failed." : err);
    return;
  }
  status_.set_text(Glib::ustring("Removed ") + row->display() + ".");
}

void Window::on_about()
{
  AboutDialog dlg(*this);
  dlg.run();
}

void Window::on_quit()
{
  hide();
}

}  // namespace sideboard
