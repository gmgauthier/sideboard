/* SPDX-License-Identifier: Unlicense */

#include "discover.hpp"
#include "check.hpp"

#include <algorithm>
#include <string>
#include <vector>

int main()
{
  CHECK(sideboard::ini_display("display=Write-It\n") == "Write-It");
  CHECK(sideboard::ini_display("# note\n\ndisplay=Kablamo!\nsummary=nope\n") == "Kablamo!");
  CHECK(sideboard::ini_display("display = YOLO-dex\r\n") == "YOLO-dex");
  CHECK(sideboard::ini_display("name=Write-It\n").empty());

  CHECK(sideboard::topic_json_has_guest("{\"names\":[\"other\",\"lcos-guest\"]}"));
  CHECK(!sideboard::topic_json_has_guest("{\"names\":[\"other\"]}"));
  CHECK(!sideboard::topic_json_has_guest("{\"topics\":[\"lcos-guest\"]}"));

  const char* json =
      "["
      "{\"name\":\"write-it\",\"description\":\"A word processor.\",\"fork\":false,"
      "\"created_at\":\"2026-10-07T21:11:59Z\",\"default_branch\":\"master\","
      "\"topics\":[\"lcos-guest\"]},"
      "{\"name\":\"cli-visualizer\",\"description\":\"not a guest\",\"fork\":false,"
      "\"created_at\":\"2020-01-01T00:00:00Z\",\"default_branch\":\"master\",\"topics\":[]},"
      "{\"name\":\"earblaster\",\"description\":\"Media player.\",\"fork\":false,"
      "\"created_at\":\"2026-09-08T08:14:55Z\",\"default_branch\":\"master\","
      "\"topics\":[\"lcos-guest\"]},"
      "{\"name\":\"sideboard\",\"description\":\"cabinet\",\"fork\":false,"
      "\"created_at\":\"2026-09-18T17:57:36Z\",\"default_branch\":\"master\","
      "\"topics\":[\"lcos-guest\"]},"
      "{\"name\":\"xepher\",\"description\":\"fork\",\"fork\":true,"
      "\"created_at\":\"2018-01-01T00:00:00Z\",\"default_branch\":\"master\","
      "\"topics\":[\"lcos-guest\"]}"
      "]";
  const std::vector<sideboard::ListedRepo> repos = sideboard::parse_user_repos(json);
  CHECK(repos.size() == 5);
  std::vector<std::string> guests;
  for (const sideboard::ListedRepo& repo : repos) {
    if (sideboard::listed_repo_is_guest(repo))
      guests.push_back(repo.name);
  }
  CHECK(guests.size() == 2);
  CHECK(guests[0] == "write-it" || guests[1] == "write-it");
  CHECK(guests[0] == "earblaster" || guests[1] == "earblaster");

  std::vector<sideboard::ListedRepo> ordered;
  for (const sideboard::ListedRepo& repo : repos) {
    if (sideboard::listed_repo_is_guest(repo))
      ordered.push_back(repo);
  }
  std::sort(ordered.begin(), ordered.end(),
            [](const sideboard::ListedRepo& a, const sideboard::ListedRepo& b) {
              if (a.created != b.created)
                return a.created < b.created;
              return a.name < b.name;
            });
  CHECK(ordered.size() == 2);
  CHECK(ordered[0].name == "earblaster");
  CHECK(ordered[1].name == "write-it");
  CHECK(ordered[0].description == "Media player.");

  sideboard::SlateGuest slate;
  slate.package = "write-it";
  slate.display = "Write-It";
  slate.summary = "A word processor.";
  slate.created = "2026-10-07T21:11:59Z";
  sideboard::set_catalog(sideboard::apps_from_slate({slate}));
  const std::vector<sideboard::App>& apps = sideboard::catalog();
  CHECK(apps.size() == 2);
  CHECK(apps[0].display == "Write-It");
  CHECK(apps[0].package == "write-it");
  CHECK(apps[0].repo == "write-it");
  CHECK(apps[1].package == "sideboard");

  sideboard::App bogus;
  bogus.package = "../evil";
  bogus.display = "Evil";
  sideboard::set_catalog({bogus});
  CHECK(sideboard::catalog().size() == 1);
  CHECK(sideboard::catalog()[0].package == "sideboard");

  return suite_test::done("discover");
}
