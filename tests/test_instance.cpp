/* SPDX-License-Identifier: Unlicense */

#include "instance.hpp"
#include "check.hpp"

#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>
#include <string>

namespace {

bool exists(const std::string& path)
{
  struct stat st;
  return ::lstat(path.c_str(), &st) == 0;
}

}  // namespace

int main()
{
  char tmpl[] = "/tmp/sideboard-instance-XXXXXX";
  const char* dir = ::mkdtemp(tmpl);
  CHECK(dir != nullptr);
  if (!dir)
    return suite_test::done("instance");

  {
    sideboard::InstanceGuard primary(dir);
    CHECK(primary.take_lock());
    CHECK(primary.listen());
    CHECK(exists(primary.socket_path()));

    {
      /* Second launch: no lock, raises the primary, then exits. */
      sideboard::InstanceGuard second(dir);
      CHECK(!second.take_lock());
      CHECK(second.send_present());
      second.close();
    }
    CHECK(exists(primary.socket_path()));

    /* Third launch can still reach the primary. */
    sideboard::InstanceGuard third(dir);
    CHECK(!third.take_lock());
    CHECK(third.send_present());
  }

  /* The primary removes its own socket on exit. */
  CHECK(!exists(std::string(dir) + "/sideboard.sock"));

  ::unlink((std::string(dir) + "/sideboard.lock").c_str());
  ::rmdir(dir);
  return suite_test::done("instance");
}
