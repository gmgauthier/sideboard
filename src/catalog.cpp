/* SPDX-License-Identifier: Unlicense */

#include "catalog.hpp"

#include <cctype>

namespace sideboard {
namespace {

std::vector<App>& apps()
{
  static std::vector<App> rows{sideboard_app()};
  return rows;
}

}  // namespace

bool package_name_ok(const std::string& name)
{
  if (name.empty() || name.size() > 64)
    return false;
  const unsigned char first = static_cast<unsigned char>(name[0]);
  if (!std::islower(first) && !std::isdigit(first))
    return false;
  for (char c : name) {
    const unsigned char u = static_cast<unsigned char>(c);
    if (!std::islower(u) && !std::isdigit(u) && c != '-')
      return false;
  }
  return true;
}

App sideboard_app()
{
  App app;
  app.id = "sideboard";
  app.display = "Sideboard";
  app.package = "sideboard";
  app.owner = "gmgauthier";
  app.repo = "sideboard";
  app.summary = "Guest-suite cabinet for LCOS. Not an official LCOS updater.";
  return app;
}

const std::vector<App>& catalog()
{
  return apps();
}

void set_catalog(std::vector<App> guests)
{
  std::vector<App> rows;
  for (auto& app : guests) {
    if (!package_name_ok(app.package) || app.package == "sideboard" || app.display.empty())
      continue;
    app.id = app.package;
    app.owner = "gmgauthier";
    app.repo = app.package;
    rows.push_back(std::move(app));
  }
  rows.push_back(sideboard_app());
  apps() = std::move(rows);
}

std::vector<App> apps_from_slate(const std::vector<SlateGuest>& slate)
{
  std::vector<App> guests;
  guests.reserve(slate.size());
  for (const SlateGuest& guest : slate) {
    App app;
    app.package = guest.package;
    app.display = guest.display;
    app.summary = guest.summary;
    guests.push_back(std::move(app));
  }
  return guests;
}

}  // namespace sideboard
