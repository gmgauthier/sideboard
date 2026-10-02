/* SPDX-License-Identifier: Unlicense */

#include "helper_core.hpp"
#include "check.hpp"

#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace {

namespace h = sideboard::helper;

std::string g_root;

struct FakeApt {
  int calls = 0;
  int rc = 0;
  std::string last_arg;
  h::AptRunner runner()
  {
    return [this](const char*, const std::string& arg, std::string& out) {
      ++calls;
      last_arg = arg;
      out = rc == 0 ? "" : "E: fake apt failure\n";
      return rc;
    };
  }
};

/* Builds ROOT/NAME from a minimal control file with Package: PACKAGE. */
std::string make_deb(const std::string& package, const std::string& name)
{
  const std::string tree = g_root + "/tree-" + package;
  const std::string out = g_root + "/" + name;
  const std::string mk = "mkdir -p '" + tree + "/DEBIAN'";
  if (std::system(mk.c_str()) != 0)
    return {};
  std::ofstream ctl(tree + "/DEBIAN/control");
  ctl << "Package: " << package << "\nVersion: 1.0.0-1\nArchitecture: amd64\n"
      << "Maintainer: Test <test@example.com>\nDescription: test package\n";
  ctl.close();
  const std::string build =
      "dpkg-deb --root-owner-group --build '" + tree + "' '" + out + "' >/dev/null 2>&1";
  if (std::system(build.c_str()) != 0)
    return {};
  return out;
}

void test_install_only_catalog_packages()
{
  const std::string cache = g_root + "/cache-catalog";

  /* A deb whose Package: is not in the catalog is refused, whatever its file name says. */
  const std::string evil = make_deb("evil", "tally_9.9.9-1_amd64.deb");
  CHECK(!evil.empty());
  FakeApt apt;
  h::Result r = h::install_deb(evil.c_str(), cache, apt.runner());
  CHECK(r.code != 0);
  CHECK(apt.calls == 0);

  /* A catalog package still installs. */
  const std::string good = make_deb("tally", "tally_1.0.0-1_amd64.deb");
  CHECK(!good.empty());
  FakeApt ok;
  r = h::install_deb(good.c_str(), cache, ok.runner());
  CHECK(r.code == 0);
  CHECK(ok.calls == 1);

  /* Something that is not a deb at all is refused. */
  const std::string junk = g_root + "/junk_1_amd64.deb";
  std::ofstream(junk) << "not a deb";
  FakeApt none;
  r = h::install_deb(junk.c_str(), cache, none.runner());
  CHECK(r.code != 0);
  CHECK(none.calls == 0);
}

void test_remove_only_catalog_packages()
{
  FakeApt apt;
  CHECK(h::remove_package("coreutils", apt.runner()).code == 2);
  CHECK(apt.calls == 0);
  CHECK(h::remove_package("tally", apt.runner()).code == 0);
  CHECK(apt.calls == 1);
}

}  // namespace

int main()
{
  char tmpl[] = "/tmp/sideboard-helper-XXXXXX";
  const char* dir = ::mkdtemp(tmpl);
  CHECK(dir != nullptr);
  if (!dir)
    return suite_test::done("helper");
  g_root = dir;

  test_install_only_catalog_packages();
  test_remove_only_catalog_packages();

  const std::string rm = "rm -rf '" + g_root + "'";
  if (std::system(rm.c_str()) != 0)
    std::cerr << "could not remove " << g_root << "\n";
  return suite_test::done("helper");
}
