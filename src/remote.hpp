/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <atomic>
#include <string>

namespace sideboard {

struct Remote {
  bool ok = false;
  bool cached = false;
  bool no_release = false;
  bool rate_limited = false;
  std::string error;
  std::string tag;
  std::string upstream;
  std::string debian;
  std::string url;
  std::string digest;
  long size = 0;
};

/* CANCEL, when set by another thread, aborts the request. */
std::string http_get(const std::string& url, std::string& error,
                     const std::atomic<bool>* cancel = nullptr);
Remote fetch_latest(const char* owner, const char* repo, const char* package,
                    const std::atomic<bool>* cancel = nullptr);
Remote parse_latest_release(const std::string& json, const char* package);

}  // namespace sideboard
