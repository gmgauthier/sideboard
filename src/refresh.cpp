/* SPDX-License-Identifier: Unlicense */

#include "refresh.hpp"

namespace sideboard {

RefreshMerge merge_refresh(CatalogCache& cache, const std::vector<std::string>& packages,
                           const std::vector<Remote>& results)
{
  RefreshMerge out;
  for (std::size_t i = 0; i < packages.size(); ++i) {
    const std::string& pkg = packages[i];
    Remote rem = (i < results.size()) ? results[i] : Remote{};
    if (rem.ok) {
      rem.cached = false;
      cache.apps[pkg] = rem;
      out.any_live = true;
    } else if (rem.no_release) {
      /* The repo answered 404: forget the old release so it is not offered again. */
      cache.apps.erase(pkg);
      out.any_live = true;
    } else {
      /* The check failed: show the previous release, marked as cached. */
      auto it = cache.apps.find(pkg);
      if (it != cache.apps.end()) {
        rem = it->second;
        rem.cached = true;
        rem.ok = true;
        out.any_cached = true;
      }
    }
    out.rows.push_back(rem);
  }
  return out;
}

std::string row_status_text(const std::string& text, bool cached)
{
  return cached ? text + " (cached)" : text;
}

}  // namespace sideboard
