/* SPDX-License-Identifier: Unlicense */

#include "compare.hpp"

#include <glibmm.h>

#include <sys/wait.h>

#include <vector>

namespace sideboard {

bool version_older(const std::string& installed_debian, const std::string& available_debian)
{
  if (installed_debian.empty() || available_debian.empty())
    return false;
  std::vector<std::string> argv = {"dpkg", "--compare-versions", installed_debian, "lt",
                                   available_debian};
  int wait_status = 0;
  try {
    Glib::spawn_sync(
        "", argv,
        Glib::SPAWN_SEARCH_PATH | Glib::SPAWN_STDOUT_TO_DEV_NULL | Glib::SPAWN_STDERR_TO_DEV_NULL,
        Glib::SlotSpawnChildSetup(), nullptr, nullptr, &wait_status);
  } catch (const Glib::Error&) {
    return false;
  }
  return WIFEXITED(wait_status) && WEXITSTATUS(wait_status) == 0;
}

}  // namespace sideboard
