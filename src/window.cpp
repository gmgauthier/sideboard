/* SPDX-License-Identifier: Unlicense */

#include "window.hpp"
#include "about_dialog.hpp"
#include "cache.hpp"
#include "catalog.hpp"
#include "compare.hpp"
#include "local.hpp"
#include "paths.hpp"

namespace sideboard {
namespace {

const int kColApp = 150;
const int kColVer = 88;
const int kColStatus = 140;
const int kColAction = 108;

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
      : package_(app.package)
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

    box_.pack_start(name_, Gtk::PACK_SHRINK);
    box_.pack_start(installed_, Gtk::PACK_SHRINK);
    box_.pack_start(available_, Gtk::PACK_SHRINK);
    box_.pack_start(status_, Gtk::PACK_EXPAND_WIDGET);
    box_.pack_start(action_, Gtk::PACK_SHRINK);
    add(box_);
    apply(query_installed(app.package), nullptr);
    show_all();
  }

  const char* package() const
  {
    return package_;
  }

  void apply(const Installed& inst, const Remote* remote)
  {
    installed_.set_text(inst.present ? Glib::ustring(inst.upstream) : Glib::ustring("—"));
    set_status_class(status_, nullptr);
    action_.set_sensitive(false);

    if (!remote) {
      available_.set_text("—");
      if (inst.present) {
        status_.set_text("—");
        action_.set_label("—");
      } else {
        status_.set_text("Not installed");
        action_.set_label("Install");
      }
      return;
    }
    if (!remote->ok) {
      available_.set_text(remote->upstream.empty() ? "—" : remote->upstream);
      status_.set_text("Error");
      set_status_class(status_, "sideboard-status-error");
      action_.set_label(inst.present ? "Upgrade" : "Install");
      return;
    }
    available_.set_text(remote->upstream);
    if (!inst.present) {
      status_.set_text("Not installed");
      action_.set_label("Install");
      return;
    }
    if (version_older(inst.debian, remote->debian)) {
      status_.set_text("Update available");
      set_status_class(status_, "sideboard-status-update");
      action_.set_label("Upgrade");
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
  }

 private:
  const char* package_ = nullptr;
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
  set_default_size(680, 440);
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
  install_all_item_->set_sensitive(false);
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
  root_.pack_start(cols_, Gtk::PACK_SHRINK);

  list_.set_selection_mode(Gtk::SELECTION_SINGLE);
  list_.get_style_context()->add_class("sideboard-list");
  scroll_.add(list_);
  scroll_.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
  scroll_.set_shadow_type(Gtk::SHADOW_IN);
  scroll_.set_hexpand(true);
  scroll_.set_vexpand(true);
  scroll_.set_margin_start(8);
  scroll_.set_margin_end(8);
  root_.pack_start(scroll_, Gtk::PACK_EXPAND_WIDGET);

  last_checked_.set_text("Last checked: —");
  last_checked_.set_xalign(0);
  last_checked_.set_hexpand(true);
  refresh_.signal_clicked().connect(sigc::mem_fun(*this, &Window::on_refresh));
  install_all_.set_sensitive(false);
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
}

void Window::set_busy(bool on)
{
  busy_ = on;
  refresh_.set_sensitive(!on);
  if (refresh_item_)
    refresh_item_->set_sensitive(!on);
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
    if (!results[i].ok) {
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
    } else {
      auto it = cache.apps.find(apps[i].package);
      if (it != cache.apps.end()) {
        rem = it->second;
        rem.cached = true;
        rem.ok = true;
      }
    }
    const bool show = rem.ok || !rem.error.empty();
    rows_[i]->apply(query_installed(apps[i].package), show ? &rem : nullptr);
  }
  if (live_ok > 0) {
    save_cache(cache);
    save_last_check(cache.checked);
    last_checked_.set_text("Last checked: " + cache.checked);
    if (avail_head_)
      avail_head_->set_text("Available");
  }
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
