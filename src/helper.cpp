/* SPDX-License-Identifier: Unlicense */

#include "helper_core.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

const char* kCacheDir = "/var/cache/sideboard";

[[noreturn]] void die(int code, const char* msg)
{
  std::fprintf(stdout, "ERR %s\n", msg);
  std::fflush(stdout);
  std::_Exit(code);
}

int report(const sideboard::helper::Result& r)
{
  if (r.code != 0)
    die(r.code, r.message.c_str());
  std::fprintf(stdout, "OK\n");
  std::fflush(stdout);
  return 0;
}

}  // namespace

int main(int argc, char** argv)
{
  if (argc != 3 && argc != 4)
    die(2, "usage: sideboard-helper install /absolute/path.deb [sha256:HEX] | remove package");

  if (std::strcmp(argv[1], "remove") == 0 && argc == 3)
    return report(sideboard::helper::remove_package(argv[2], sideboard::helper::run_apt));

  if (std::strcmp(argv[1], "install") != 0)
    die(2, "usage: sideboard-helper install /absolute/path.deb [sha256:HEX] | remove package");

  const std::string digest = argc == 4 ? argv[3] : "";
  return report(
      sideboard::helper::install_deb(argv[2], kCacheDir, sideboard::helper::run_apt, digest));
}
