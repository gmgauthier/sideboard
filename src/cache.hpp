/* SPDX-License-Identifier: Unlicense */

#pragma once

#include "catalog.hpp"
#include "remote.hpp"

#include <map>
#include <string>
#include <vector>

namespace sideboard {

struct CatalogCache {
  std::string checked;                /* display stamp, e.g. 18 Sep 2026, 12:18 */
  std::map<std::string, Remote> apps; /* keyed by package */
  std::vector<SlateGuest> slate;      /* last guest list, Sideboard excluded */
};

std::string cache_path();
std::string debs_dir();
std::string config_path();
CatalogCache load_cache();
void save_cache(const CatalogCache& cache);
void save_last_check(const std::string& stamp);
std::string load_last_check();
std::string now_stamp();

}  // namespace sideboard
