/* SPDX-License-Identifier: Unlicense */

#pragma once

#include "cache.hpp"
#include "remote.hpp"

#include <string>
#include <vector>

namespace sideboard {

struct RefreshMerge {
  std::vector<Remote> rows; /* what each catalog row shows, in catalog order */
  bool any_live = false;    /* at least one repo answered; the cache should be saved */
  bool any_cached = false;  /* at least one row is showing a cached release */
};

/* Folds one Refresh into CACHE. PACKAGES and RESULTS are in catalog order. */
RefreshMerge merge_refresh(CatalogCache& cache, const std::vector<std::string>& packages,
                           const std::vector<Remote>& results);

/* Status text for a row, marked when it comes from the cache rather than a live check. */
std::string row_status_text(const std::string& text, bool cached);

}  // namespace sideboard
