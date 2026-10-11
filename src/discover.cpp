/* SPDX-License-Identifier: Unlicense */

#include "discover.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace sideboard {
namespace {

const char* kTopic = "lcos-guest";

TopicProbe& topic_probe()
{
  static TopicProbe probe;
  return probe;
}

void skip_ws(const std::string& s, std::size_t& i)
{
  while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i])))
    ++i;
}

bool parse_string(const std::string& s, std::size_t& i, std::string& out)
{
  if (i >= s.size() || s[i] != '"')
    return false;
  ++i;
  out.clear();
  while (i < s.size()) {
    const char c = s[i++];
    if (c == '"')
      return true;
    if (c == '\\' && i < s.size()) {
      const char e = s[i++];
      if (e == 'n')
        out += '\n';
      else if (e == 't')
        out += '\t';
      else
        out += e;
    } else {
      out += c;
    }
  }
  return false;
}

std::size_t match_brace(const std::string& s, std::size_t open)
{
  int depth = 0;
  bool in_str = false;
  bool esc = false;
  for (std::size_t i = open; i < s.size(); ++i) {
    const char c = s[i];
    if (in_str) {
      if (esc)
        esc = false;
      else if (c == '\\')
        esc = true;
      else if (c == '"')
        in_str = false;
      continue;
    }
    if (c == '"')
      in_str = true;
    else if (c == '{')
      ++depth;
    else if (c == '}') {
      --depth;
      if (depth == 0)
        return i;
    }
  }
  return std::string::npos;
}

bool find_key(const std::string& s, const char* key, std::size_t& pos)
{
  const std::string pat = std::string("\"") + key + "\"";
  const auto p = s.find(pat);
  if (p == std::string::npos)
    return false;
  pos = p + pat.size();
  skip_ws(s, pos);
  if (pos >= s.size() || s[pos] != ':')
    return false;
  ++pos;
  skip_ws(s, pos);
  return true;
}

bool key_string(const std::string& s, const char* key, std::string& out)
{
  std::size_t pos = 0;
  if (!find_key(s, key, pos))
    return false;
  return parse_string(s, pos, out);
}

bool key_bool(const std::string& s, const char* key, bool& out)
{
  std::size_t pos = 0;
  if (!find_key(s, key, pos))
    return false;
  if (s.compare(pos, 4, "true") == 0) {
    out = true;
    return true;
  }
  if (s.compare(pos, 5, "false") == 0) {
    out = false;
    return true;
  }
  return false;
}

bool key_string_array(const std::string& s, const char* key, std::vector<std::string>& out)
{
  std::size_t pos = 0;
  if (!find_key(s, key, pos) || pos >= s.size() || s[pos] != '[')
    return false;
  ++pos;
  out.clear();
  while (pos < s.size()) {
    skip_ws(s, pos);
    if (pos >= s.size())
      return false;
    if (s[pos] == ']')
      return true;
    if (s[pos] == ',') {
      ++pos;
      continue;
    }
    std::string item;
    if (!parse_string(s, pos, item))
      return false;
    out.push_back(std::move(item));
  }
  return false;
}

std::string trim(std::string s)
{
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
    s.erase(s.begin());
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
    s.pop_back();
  return s;
}

std::string one_line(const std::string& in)
{
  std::string out;
  out.reserve(in.size());
  for (char c : in) {
    if (c == '\n' || c == '\r' || c == '\t')
      out += ' ';
    else
      out += c;
  }
  out = trim(out);
  if (out.size() > 240)
    out.resize(240);
  return out;
}

bool branch_ok(const std::string& branch)
{
  if (branch.empty() || branch.size() > 128 || branch.find("..") != std::string::npos)
    return false;
  for (char c : branch) {
    const unsigned char u = static_cast<unsigned char>(c);
    if (!std::isalnum(u) && c != '.' && c != '_' && c != '-' && c != '/')
      return false;
  }
  return true;
}

bool array_has(const std::string& json, const char* key, const char* needle)
{
  std::vector<std::string> values;
  if (!key_string_array(json, key, values))
    return false;
  for (const std::string& value : values) {
    if (value == needle)
      return true;
  }
  return false;
}

ListedRepo parse_repo_object(const std::string& obj)
{
  ListedRepo repo;
  key_string(obj, "name", repo.name);
  key_string(obj, "description", repo.description);
  key_string(obj, "created_at", repo.created);
  key_string(obj, "default_branch", repo.branch);
  repo.saw_fork = key_bool(obj, "fork", repo.fork);
  key_string_array(obj, "topics", repo.topics);
  return repo;
}

bool no_deb(const Remote& remote)
{
  return remote.no_release || remote.error == "No amd64 .deb asset" ||
         remote.error == "No assets" || remote.error == "No tag_name";
}

bool rate_limited(const std::string& err)
{
  return err == "HTTP 403" || err == "HTTP 429";
}

}  // namespace

std::string ini_display(const std::string& text)
{
  std::size_t i = 0;
  while (i < text.size()) {
    std::size_t end = text.find('\n', i);
    if (end == std::string::npos)
      end = text.size();
    std::string line = text.substr(i, end - i);
    i = end + (end < text.size() ? 1 : 0);
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    line = trim(line);
    if (line.empty() || line[0] == '#' || line[0] == ';')
      continue;
    const auto eq = line.find('=');
    if (eq == std::string::npos)
      continue;
    if (trim(line.substr(0, eq)) != "display")
      continue;
    std::string value = trim(line.substr(eq + 1));
    if (value.empty())
      return {};
    if (value.size() > 80)
      value.resize(80);
    return value;
  }
  return {};
}

std::vector<ListedRepo> parse_user_repos(const std::string& json)
{
  std::vector<ListedRepo> repos;
  std::size_t i = json.find('[');
  if (i == std::string::npos)
    return repos;
  ++i;
  while (i < json.size()) {
    skip_ws(json, i);
    if (i >= json.size() || json[i] == ']')
      break;
    if (json[i] == ',') {
      ++i;
      continue;
    }
    if (json[i] != '{')
      break;
    const std::size_t end = match_brace(json, i);
    if (end == std::string::npos)
      break;
    ListedRepo repo = parse_repo_object(json.substr(i, end - i + 1));
    if (!repo.name.empty())
      repos.push_back(std::move(repo));
    i = end + 1;
  }
  return repos;
}

bool listed_repo_is_guest(const ListedRepo& repo)
{
  if (!repo.saw_fork || repo.fork || repo.name == "sideboard" || !package_name_ok(repo.name))
    return false;
  for (const std::string& topic : repo.topics) {
    if (topic == kTopic)
      return true;
  }
  return false;
}

bool topic_json_has_guest(const std::string& json)
{
  return array_has(json, "names", kTopic);
}

DiscoverResult discover_guests(const std::atomic<bool>* cancel)
{
  DiscoverResult out;
  std::vector<ListedRepo> repos;
  for (int page = 1; page <= 5; ++page) {
    if (cancel && cancel->load()) {
      out.error = "Cancelled";
      return out;
    }
    std::string err;
    const std::string url =
        "https://api.github.com/users/gmgauthier/repos?per_page=100&type=owner&page=" +
        std::to_string(page);
    const std::string body = http_get(url, err, cancel);
    if (body.empty()) {
      out.rate_limited = rate_limited(err);
      out.error = err.empty() ? "Empty response" : err;
      return out;
    }
    if (body.find('[') == std::string::npos) {
      out.error = "Repo list was not a JSON array";
      return out;
    }
    const std::vector<ListedRepo> page_repos = parse_user_repos(body);
    if (page_repos.empty() && body.find("\"full_name\"") != std::string::npos) {
      out.error = "Could not read the repo list";
      return out;
    }
    repos.insert(repos.end(), page_repos.begin(), page_repos.end());
    if (page_repos.size() < 100)
      break;
  }

  std::vector<ListedRepo> guests;
  for (const ListedRepo& repo : repos) {
    if (listed_repo_is_guest(repo))
      guests.push_back(repo);
  }
  std::sort(guests.begin(), guests.end(), [](const ListedRepo& a, const ListedRepo& b) {
    if (a.created != b.created)
      return a.created < b.created;
    return a.name < b.name;
  });

  for (const ListedRepo& repo : guests) {
    if (cancel && cancel->load()) {
      out.error = "Cancelled";
      out.guests.clear();
      return out;
    }
    const std::string branch = branch_ok(repo.branch) ? repo.branch : "master";
    const std::string url = "https://api.github.com/repos/gmgauthier/" + repo.name +
                            "/contents/sideboard.ini?ref=" + branch;
    std::string err;
    const std::string body = http_get(url, err, cancel, true, "application/vnd.github.raw");
    if (body.empty()) {
      if (rate_limited(err)) {
        out.rate_limited = true;
        out.error = err;
        out.guests.clear();
        return out;
      }
      continue;
    }
    const std::string display = ini_display(body);
    if (display.empty())
      continue;

    const Remote remote = fetch_latest("gmgauthier", repo.name.c_str(), repo.name.c_str(), cancel);
    if (remote.rate_limited) {
      out.rate_limited = true;
      out.error = remote.error.empty() ? "HTTP 403" : remote.error;
      out.guests.clear();
      return out;
    }
    if (no_deb(remote))
      continue;

    DiscoveredGuest guest;
    guest.app.id = repo.name;
    guest.app.display = display;
    guest.app.package = repo.name;
    guest.app.owner = "gmgauthier";
    guest.app.repo = repo.name;
    guest.app.summary = one_line(repo.description);
    guest.remote = remote;
    guest.created = repo.created;
    out.guests.push_back(std::move(guest));
  }
  out.ok = true;
  return out;
}

void set_topic_probe(TopicProbe probe)
{
  topic_probe() = std::move(probe);
}

bool allowed_package(const char* pkg)
{
  if (!pkg || !package_name_ok(pkg))
    return false;
  if (std::strcmp(pkg, "sideboard") == 0)
    return true;
  if (topic_probe())
    return topic_probe()(pkg);
  std::string err;
  const std::string url = std::string("https://api.github.com/repos/gmgauthier/") + pkg + "/topics";
  const std::string body = http_get(url, err, nullptr, false);
  if (body.empty())
    return false;
  return topic_json_has_guest(body);
}

}  // namespace sideboard
