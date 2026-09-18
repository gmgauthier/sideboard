/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <functional>
#include <string>

namespace sideboard {

using DownloadProgress = std::function<void(long current, long total)>;

bool download_file(const std::string& url, const std::string& dest, std::string& error,
                   const DownloadProgress& progress = {});
std::string sha256_file(const std::string& path, std::string& error);
bool digest_matches(const std::string& digest, const std::string& hex);

}  // namespace sideboard
