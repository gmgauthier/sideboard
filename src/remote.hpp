/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <string>

namespace sideboard {

struct Remote {
  bool ok = false;
  bool cached = false;
  std::string error;
  std::string tag;
  std::string upstream;
  std::string debian;
  std::string url;
  std::string digest;
  long size = 0;
};

std::string http_get(const std::string& url, std::string& error);
Remote fetch_latest(const char* owner, const char* repo, const char* package);
Remote parse_latest_release(const std::string& json, const char* package);

}  // namespace sideboard
