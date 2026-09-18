/* SPDX-License-Identifier: Unlicense */

#include "local.hpp"

#include <glibmm.h>

#include <sys/wait.h>

namespace sideboard {

std::string display_upstream(const std::string& debian_version)
{
  std::string v = debian_version;
  while (!v.empty() && (v.back() == '\n' || v.back() == '\r'))
    v.pop_back();
  const auto dash = v.rfind('-');
  if (dash == std::string::npos || dash == 0)
    return v;
  return v.substr(0, dash);
}

Installed query_installed(const char* package)
{
  Installed out;
  if (!package || package[0] == '\0')
    return out;

  std::string stdout_buf;
  std::string stderr_buf;
  int wait_status = 0;
  const std::string cmd = std::string("dpkg-query -W -f=${Version} ") + package;
  try {
    Glib::spawn_command_line_sync(cmd, &stdout_buf, &stderr_buf, &wait_status);
  } catch (const Glib::Error&) {
    return out;
  }
  if (!WIFEXITED(wait_status) || WEXITSTATUS(wait_status) != 0)
    return out;
  if (stdout_buf.empty())
    return out;

  out.present = true;
  out.debian = stdout_buf;
  while (!out.debian.empty() && (out.debian.back() == '\n' || out.debian.back() == '\r'))
    out.debian.pop_back();
  out.upstream = display_upstream(out.debian);
  return out;
}

}  // namespace sideboard
