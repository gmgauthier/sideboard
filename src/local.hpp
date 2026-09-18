/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <string>

namespace sideboard {

struct Installed {
  bool present = false;
  std::string debian;
  std::string upstream;
};

Installed query_installed(const char* package);
std::string display_upstream(const std::string& debian_version);
std::string dpkg_architecture();

}  // namespace sideboard
