/* SPDX-License-Identifier: Unlicense */

#include "about_dialog.hpp"
#include "catalog.hpp"
#include "config.hpp"

namespace sideboard {

AboutDialog::AboutDialog(Gtk::Window& parent)
    : Gtk::Dialog("About Sideboard", parent, true)
{
  set_resizable(false);
  add_button("_OK", Gtk::RESPONSE_OK);
  set_default_response(Gtk::RESPONSE_OK);

  auto* box = get_content_area();
  box->set_border_width(16);
  box->set_spacing(10);

  auto* title = Gtk::manage(new Gtk::Label());
  title->set_markup("<b>Sideboard " VERSION "</b>");
  box->pack_start(*title, Gtk::PACK_SHRINK);

  auto* line = Gtk::manage(
      new Gtk::Label("Sideboard — guest software for The Lunduke Computer Operating System."));
  line->set_line_wrap(true);
  line->set_max_width_chars(56);
  box->pack_start(*line, Gtk::PACK_SHRINK);

  auto* guest = Gtk::manage(new Gtk::Label(
      "Third-party software written for LCOS. Not an official LCOS updater. Not a store."));
  guest->set_line_wrap(true);
  guest->set_max_width_chars(56);
  box->pack_start(*guest, Gtk::PACK_SHRINK);

  std::size_t n = 0;
  const App* apps = catalog(&n);
  std::string names;
  for (std::size_t i = 0; i < n; ++i) {
    if (i)
      names += ", ";
    names += apps[i].display;
  }
  auto* list = Gtk::manage(new Gtk::Label("Guest apps: " + names + "."));
  list->set_line_wrap(true);
  list->set_max_width_chars(56);
  box->pack_start(*list, Gtk::PACK_SHRINK);

  auto* gh = Gtk::manage(new Gtk::Label("https://github.com/gmgauthier"));
  box->pack_start(*gh, Gtk::PACK_SHRINK);

  auto* license = Gtk::manage(new Gtk::Label("Released under The Unlicense."));
  box->pack_start(*license, Gtk::PACK_SHRINK);

  show_all_children();
}

}  // namespace sideboard
