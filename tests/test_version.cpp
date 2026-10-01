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

  std::size_t count = 0;
  const sideboard::App* apps = sideboard::catalog(&count);
  CHECK(apps != nullptr);
  CHECK(count == 11);
  bool saw_listen = false;
  for (std::size_t i = 0; i < count; ++i) {
    CHECK(apps[i].package != nullptr);
    CHECK(apps[i].package[0] != '\0');
    if (std::string(apps[i].id) == "listenomatic") {
      saw_listen = true;
      CHECK(std::string(apps[i].package) == "listenomatic");
      CHECK(std::string(apps[i].repo) == "listenomatic");
    }
  }
  CHECK(saw_listen);

  return suite_test::done("version");
}
