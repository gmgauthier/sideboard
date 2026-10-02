/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <string>
#include <vector>

namespace sideboard {

/* pkexec argv for "sideboard-helper install DEB [DIGEST]". The helper checks DIGEST
   against its own root-owned copy, so a swap after the GUI's check is refused. */
inline std::vector<std::string> helper_install_argv(const std::string& helper,
                                                    const std::string& deb,
                                                    const std::string& digest)
{
  std::vector<std::string> argv = {"pkexec", helper, "install", deb};
  if (digest.empty())
    return argv;
  /* The GUI accepts a bare hex digest too; the helper wants the sha256: form. */
  argv.push_back(digest.compare(0, 7, "sha256:") == 0 ? digest : "sha256:" + digest);
  return argv;
}

}  // namespace sideboard
