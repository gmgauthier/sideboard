/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <string>
#include <vector>

namespace sideboard {

struct App {
  std::string id;
  std::string display;
  std::string package;
  std::string owner;
  std::string repo;
  std::string summary;
};

/* A guest remembered from the last successful list, so the window can open offline. */
struct SlateGuest {
  std::string package;
  std::string display;
  std::string summary;
  std::string created;
};

/* Repository names that are safe to put in a GitHub URL path. */
bool package_name_ok(const std::string& name);

/* The fixed last row. Sideboard does not carry the lcos-guest topic. */
App sideboard_app();

/* Guests followed by Sideboard. Starts as Sideboard alone. */
const std::vector<App>& catalog();

/* Replace the guest rows. Sideboard is always pinned last. */
void set_catalog(std::vector<App> guests);

/* Guest rows from the cache. Sideboard is not included. */
std::vector<App> apps_from_slate(const std::vector<SlateGuest>& slate);

}  // namespace sideboard
