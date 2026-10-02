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

mode_t mode_of(const std::string& path)
{
  struct stat st;
  if (::lstat(path.c_str(), &st) != 0)
    return 0;
  return st.st_mode & 07777;
}

bool exists(const std::string& path)
{
  struct stat st;
  return ::lstat(path.c_str(), &st) == 0;
}

void test_install_refuses_symlink_and_keeps_copy_private()
{
  const std::string cache = g_root + "/cache-private";

  /* A symlink is refused and nothing is copied, even when it points at a catalog deb. */
  const std::string secret = g_root + "/secret";
  std::ofstream(secret) << "root:secret:hash\n";
  const std::string link = g_root + "/x_1_amd64.deb";
  CHECK(::symlink(secret.c_str(), link.c_str()) == 0);
  FakeApt plain;
  CHECK(h::install_deb(link.c_str(), cache, plain.runner()).code != 0);
  CHECK(plain.calls == 0);
  CHECK(!exists(cache + "/x_1_amd64.deb"));
  const std::string target = make_deb("tally", "target.deb");
  const std::string deb_link = g_root + "/tally_2.0.0-1_amd64.deb";
  CHECK(::symlink(target.c_str(), deb_link.c_str()) == 0);
  FakeApt apt;
  h::Result r = h::install_deb(deb_link.c_str(), cache, apt.runner());
  CHECK(r.code != 0);
  CHECK(apt.calls == 0);
  CHECK(!exists(cache + "/tally_2.0.0-1_amd64.deb"));

  /* A real install: the cache directory and the copy are readable only by their owner. */
  const std::string good = make_deb("tally", "tally_1.0.1-1_amd64.deb");
  mode_t dir_mode = 0;
  mode_t file_mode = 0;
  FakeApt seen;
  h::AptRunner inspect = [&](const char* op, const std::string& arg, std::string& out) {
    dir_mode = mode_of(cache);
    file_mode = mode_of(arg);
    return seen.runner()(op, arg, out);
  };
  r = h::install_deb(good.c_str(), cache, inspect);
  CHECK(r.code == 0);
  CHECK(seen.calls == 1);
  CHECK(dir_mode == 0700);
  CHECK(file_mode == 0600);

  /* A cache directory left 0755 by an older helper is tightened. */
  const std::string old_cache = g_root + "/cache-old";
  CHECK(::mkdir(old_cache.c_str(), 0755) == 0);
  CHECK(::chmod(old_cache.c_str(), 0755) == 0);
  FakeApt again;
  r = h::install_deb(good.c_str(), old_cache, again.runner());
  CHECK(r.code == 0);
  CHECK(mode_of(old_cache) == 0700);

  /* When apt-get fails, the copy is removed. */
  FakeApt failing;
  failing.rc = 100;
  r = h::install_deb(good.c_str(), cache, failing.runner());
  CHECK(r.code == 1);
  CHECK(failing.calls == 1);
  CHECK(!exists(failing.last_arg));
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
  test_install_refuses_symlink_and_keeps_copy_private();

  const std::string rm = "rm -rf '" + g_root + "'";
  if (std::system(rm.c_str()) != 0)
    std::cerr << "could not remove " << g_root << "\n";
  return suite_test::done("helper");
}
