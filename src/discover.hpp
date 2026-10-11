/* SPDX-License-Identifier: Unlicense */

#pragma once

#include "catalog.hpp"
#include "remote.hpp"

#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace sideboard {

struct ListedRepo {
  std::string name;
  std::string description;
  std::string created;
  std::string branch;
  bool fork = false;
  bool saw_fork = false;
  std::vector<std::string> topics;
};

struct DiscoveredGuest {
  App app;
  Remote remote;
  std::string created;
};

struct DiscoverResult {
  bool ok = false;
  bool rate_limited = false;
  std::string error;
  std::vector<DiscoveredGuest> guests;
};

/* display= value from a sideboard.ini, or empty. */
std::string ini_display(const std::string& text);

/* Public repos from GET /users/gmgauthier/repos. */
std::vector<ListedRepo> parse_user_repos(const std::string& json);

/* Topic, not a fork, not Sideboard itself, and a safe repository name. */
bool listed_repo_is_guest(const ListedRepo& repo);

/* Topics API body: {"names":["lcos-guest"]}. */
bool topic_json_has_guest(const std::string& json);

/* Guests in creation order. Repos without a display file or an amd64 .deb are omitted. */
DiscoverResult discover_guests(const std::atomic<bool>* cancel = nullptr);

/* Test double for the helper. An empty probe checks GitHub. */
using TopicProbe = std::function<bool(const std::string& package)>;
void set_topic_probe(TopicProbe probe);

/* Sideboard itself, or a package whose GitHub repo carries lcos-guest. */
bool allowed_package(const char* pkg);

}  // namespace sideboard
