/* SPDX-License-Identifier: Unlicense */

#include "refresh.hpp"
#include "check.hpp"

#include <string>
#include <vector>

namespace {

using sideboard::CatalogCache;
using sideboard::Remote;

Remote release(const std::string& upstream)
{
  Remote r;
  r.ok = true;
  r.tag = "v" + upstream;
  r.upstream = upstream;
  r.debian = upstream + "-1";
  r.url = "https://example.com/x_" + upstream + "_amd64.deb";
  return r;
}

Remote cached_release(const std::string& upstream)
{
  Remote r = release(upstream);
  r.cached = true;
  return r;
}

Remote failure(const std::string& error)
{
  Remote r;
  r.error = error;
  return r;
}

Remote not_found()
{
  Remote r;
  r.no_release = true;
  return r;
}

void test_failed_row_is_marked_cached()
{
  CatalogCache cache;
  cache.apps["alpha"] = cached_release("1.0.0");
  cache.apps["beta"] = cached_release("2.0.0");
  const std::vector<std::string> pkgs = {"alpha", "beta"};
  const auto m = sideboard::merge_refresh(cache, pkgs, {release("1.1.0"), failure("timeout")});

  CHECK(m.rows.size() == 2);
  CHECK(m.rows[0].ok && !m.rows[0].cached && m.rows[0].upstream == "1.1.0");
  /* The failed repo shows its old release, marked as cached, not as a live result. */
  CHECK(m.rows[1].upstream == "2.0.0");
  CHECK(m.rows[1].cached);
  CHECK(m.any_live);
  CHECK(m.any_cached);
  CHECK(cache.apps["alpha"].upstream == "1.1.0");
  CHECK(cache.apps["beta"].upstream == "2.0.0");
}

void test_404_drops_the_cached_release()
{
  CatalogCache cache;
  cache.apps["alpha"] = cached_release("1.0.0");
  cache.apps["gone"] = cached_release("3.0.0");
  const std::vector<std::string> pkgs = {"alpha", "gone"};
  const auto m = sideboard::merge_refresh(cache, pkgs, {release("1.0.0"), not_found()});

  CHECK(m.rows[1].no_release);
  CHECK(!m.rows[1].ok);
  /* The saved cache must not offer the withdrawn release on the next launch. */
  CHECK(cache.apps.count("gone") == 0);
  CHECK(m.any_live);
  CHECK(!m.any_cached);
}

void test_all_live_is_not_cached()
{
  CatalogCache cache;
  cache.apps["alpha"] = cached_release("1.0.0");
  const auto m = sideboard::merge_refresh(cache, {"alpha"}, {release("1.0.0")});
  CHECK(m.any_live);
  CHECK(!m.any_cached);
  CHECK(!m.rows[0].cached);
}

void test_all_failed_is_not_live()
{
  CatalogCache cache;
  cache.apps["alpha"] = cached_release("1.0.0");
  const auto m = sideboard::merge_refresh(cache, {"alpha", "beta"},
                                          {failure("HTTP 502"), failure("HTTP 502")});
  CHECK(!m.any_live);
  CHECK(m.any_cached);
  CHECK(m.rows[0].cached);
  CHECK(!m.rows[1].ok);
  CHECK(m.rows[1].error == "HTTP 502");
}

void test_row_status_text()
{
  CHECK(sideboard::row_status_text("Up to date", false) == "Up to date");
  CHECK(sideboard::row_status_text("Up to date", true) == "Up to date (cached)");
}

}  // namespace

int main()
{
  test_failed_row_is_marked_cached();
  test_404_drops_the_cached_release();
  test_all_live_is_not_cached();
  test_all_failed_is_not_live();
  test_row_status_text();
  return suite_test::done("refresh");
}
