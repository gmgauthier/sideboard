/* SPDX-License-Identifier: Unlicense */

#include "helper_core.hpp"
#include "helper_args.hpp"
#include "check.hpp"

#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <fstream>
#include <thread>
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

std::string sha256_of(const std::string& path)
{
  const std::string cmd = "sha256sum '" + path + "'";
  FILE* p = ::popen(cmd.c_str(), "r");
  if (!p)
    return {};
  char buf[65] = {0};
  const std::size_t n = std::fread(buf, 1, 64, p);
  ::pclose(p);
  return n == 64 ? std::string(buf, 64) : std::string();
}

void test_install_checks_digest_on_roots_copy()
{
  const std::string cache = g_root + "/cache-digest";
  const std::string good = make_deb("tally", "tally_1.0.2-1_amd64.deb");
  const std::string hex = sha256_of(good);
  CHECK(hex.size() == 64);

  /* The digest the GUI checked does not match the file root copied: refused. */
  const std::string other = make_deb("needle", "needle_1.0.0-1_amd64.deb");
  FakeApt swapped;
  h::Result r = h::install_deb(other.c_str(), cache, swapped.runner(), "sha256:" + hex);
  CHECK(r.code != 0);
  CHECK(swapped.calls == 0);
  CHECK(!exists(cache + "/needle_1.0.0-1_amd64.deb"));

  /* A matching digest installs, in either case. */
  FakeApt ok;
  r = h::install_deb(good.c_str(), cache, ok.runner(), "sha256:" + hex);
  CHECK(r.code == 0);
  CHECK(ok.calls == 1);
  std::string upper = hex;
  for (auto& c : upper)
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  FakeApt ok_upper;
  CHECK(h::install_deb(good.c_str(), cache, ok_upper.runner(), "sha256:" + upper).code == 0);

  /* A malformed digest is refused. */
  FakeApt bad;
  CHECK(h::install_deb(good.c_str(), cache, bad.runner(), "md5:abc").code == 2);
  CHECK(h::install_deb(good.c_str(), cache, bad.runner(), "sha256:" + hex.substr(1)).code == 2);
  CHECK(bad.calls == 0);

  /* The GUI hands the published digest to the helper. */
  const auto with = sideboard::helper_install_argv("/usr/libexec/sideboard-helper", good,
                                                   "sha256:" + hex);
  CHECK(with.size() == 5);
  CHECK(with[0] == "pkexec");
  CHECK(with[2] == "install");
  CHECK(with[3] == good);
  CHECK(with.size() == 5 && with[4] == "sha256:" + hex);
  const auto without = sideboard::helper_install_argv("/usr/libexec/sideboard-helper", good, "");
  CHECK(without.size() == 4);
  const auto bare = sideboard::helper_install_argv("/usr/libexec/sideboard-helper", good, hex);
  CHECK(bare.size() == 5 && bare[4] == "sha256:" + hex);
}

long size_of(const std::string& path)
{
  struct stat st;
  if (::stat(path.c_str(), &st) != 0)
    return -1;
  return static_cast<long>(st.st_size);
}

void test_reinstall_from_cache_keeps_the_deb()
{
  const std::string cache = g_root + "/cache-reinstall";
  const std::string good = make_deb("tally", "tally_1.0.3-1_amd64.deb");
  const long size = size_of(good);
  CHECK(size > 0);
  FakeApt first;
  CHECK(h::install_deb(good.c_str(), cache, first.runner()).code == 0);
  const std::string cached = cache + "/tally_1.0.3-1_amd64.deb";
  CHECK(size_of(cached) == size);

  /* Installing the copy a previous install left behind must not empty it. */
  long seen_size = -1;
  FakeApt again;
  h::AptRunner inspect = [&](const char* op, const std::string& arg, std::string& out) {
    seen_size = size_of(arg);
    return again.runner()(op, arg, out);
  };
  const h::Result r = h::install_deb(cached.c_str(), cache, inspect);
  CHECK(r.code == 0);
  CHECK(again.calls == 1);
  CHECK(seen_size == size);
  CHECK(size_of(cached) == size);
}

/* True once PID has exited (gone, or a zombie waiting for its new parent). */
bool process_gone(long pid)
{
  if (::kill(static_cast<pid_t>(pid), 0) != 0)
    return true;
  std::ifstream stat("/proc/" + std::to_string(pid) + "/stat");
  std::string line;
  std::getline(stat, line);
  const auto close = line.rfind(')');
  return close != std::string::npos && close + 2 < line.size() && line[close + 2] == 'Z';
}

void test_timeout_kills_the_whole_process_group()
{
  /* The shell stands in for apt-get; its background sleep stands in for dpkg. */
  std::string out;
  const auto start = std::chrono::steady_clock::now();
  const int rc =
      h::run_capture({"/bin/sh", "-c", "sleep 30 & echo $!; wait"}, 1, out);
  const double took =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  CHECK(rc == -1);
  CHECK(took < 10.0);
  const long child = std::atol(out.c_str());
  CHECK(child > 0);
  if (child <= 0)
    return;
  bool gone = false;
  for (int i = 0; i < 40 && !gone; ++i) {
    gone = process_gone(child);
    if (!gone)
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  CHECK(gone);
  if (!gone)
    ::kill(static_cast<pid_t>(child), SIGKILL);
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
  test_install_checks_digest_on_roots_copy();
  test_reinstall_from_cache_keeps_the_deb();
  test_timeout_kills_the_whole_process_group();

  const std::string rm = "rm -rf '" + g_root + "'";
  if (std::system(rm.c_str()) != 0)
    std::cerr << "could not remove " << g_root << "\n";
  return suite_test::done("helper");
}
