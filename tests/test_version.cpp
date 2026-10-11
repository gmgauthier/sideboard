/* SPDX-License-Identifier: Unlicense */

#include "catalog.hpp"
#include "compare.hpp"
#include "check.hpp"

#include <cstddef>
#include <string>

namespace sideboard {
std::string display_upstream(const std::string& debian_version);
}

int main()
{
  CHECK(!sideboard::version_older("", "1.0.1-1"));
  CHECK(!sideboard::version_older("1.0.1-1", ""));
  CHECK(!sideboard::version_older("1.0.1-1", "1.0.1-1"));
  CHECK(sideboard::version_older("1.0.0-1", "1.0.1-1"));
  CHECK(!sideboard::version_older("1.0.1-1", "1.0.0-1"));
  CHECK(sideboard::version_older("1.0.0-1", "1:1.0.0-1"));
  CHECK(sideboard::version_older("0.9.0-1", "0.10.0-1"));

  CHECK(sideboard::display_upstream("1.0.1-1") == "1.0.1");
  CHECK(sideboard::display_upstream("1.0.1") == "1.0.1");
  CHECK(sideboard::display_upstream("2:1.0.0-1") == "2:1.0.0");
  CHECK(sideboard::display_upstream("1.0-1ubuntu1") == "1.0");
  CHECK(sideboard::display_upstream("") == "");

  const std::vector<sideboard::App>& apps = sideboard::catalog();
  CHECK(apps.size() == 1);
  CHECK(apps[0].package == "sideboard");
  CHECK(sideboard::package_name_ok("write-it"));
  CHECK(!sideboard::package_name_ok("Write-It"));
  CHECK(!sideboard::package_name_ok("../evil"));
  CHECK(!sideboard::package_name_ok(""));

  return suite_test::done("version");
}
