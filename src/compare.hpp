/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <string>

namespace sideboard {

/* True when installed Debian version is older than available (dpkg comparer). */
bool version_older(const std::string& installed_debian, const std::string& available_debian);

}  // namespace sideboard
