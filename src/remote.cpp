/* SPDX-License-Identifier: Unlicense */

#include "remote.hpp"
#include "config.hpp"

#include <curl/curl.h>

#include <cctype>

namespace sideboard {
namespace {

const long kMaxBody = 2L * 1024L * 1024L;

bool is_deb_name(const std::string& name, const char* package)
{
  const std::string prefix = std::string(package) + "_";
  const std::string suffix = "_amd64.deb";
  if (name.size() <= prefix.size() + suffix.size())
    return false;
  return name.compare(0, prefix.size(), prefix) == 0 &&
         name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0;
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

bool find_key(const std::string& s, std::size_t from, const char* key, std::size_t& pos)
{
  const std::string pat = std::string("\"") + key + "\"";
  const auto p = s.find(pat, from);
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

bool key_string(const std::string& s, std::size_t from, const char* key, std::string& out)
{
  std::size_t pos = 0;
  if (!find_key(s, from, key, pos))
    return false;
  return parse_string(s, pos, out);
}

bool key_long(const std::string& s, std::size_t from, const char* key, long& out)
{
  std::size_t pos = 0;
  if (!find_key(s, from, key, pos))
    return false;
  if (pos >= s.size() || !std::isdigit(static_cast<unsigned char>(s[pos])))
    return false;
  out = 0;
  while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) {
    out = out * 10 + (s[pos] - '0');
    ++pos;
  }
  return true;
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

std::string strip_v(const std::string& tag)
{
  if (tag.size() >= 2 && (tag[0] == 'v' || tag[0] == 'V') &&
      std::isdigit(static_cast<unsigned char>(tag[1])))
    return tag.substr(1);
  return tag;
}

size_t curl_write(char* ptr, size_t size, size_t nmemb, void* userdata)
{
  auto* body = static_cast<std::string*>(userdata);
  const size_t n = size * nmemb;
  if (body->size() + n > static_cast<size_t>(kMaxBody))
    return 0;
  body->append(ptr, n);
  return n;
}

}  // namespace

std::string http_get(const std::string& url, std::string& error)
{
  error.clear();
  CURL* curl = curl_easy_init();
  if (!curl) {
    error = "curl init failed";
    return {};
  }
  std::string body;
  const std::string ua =
      std::string("sideboard/") + VERSION + " (+https://github.com/gmgauthier/sideboard)";
  struct curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "Accept: application/vnd.github+json");
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_USERAGENT, ua.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
  curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https,http");
  const CURLcode rc = curl_easy_perform(curl);
  long status = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  if (rc != CURLE_OK) {
    error = curl_easy_strerror(rc);
    return {};
  }
  if (status == 403 || status == 429) {
    error = "HTTP " + std::to_string(status);
    return {};
  }
  if (status < 200 || status >= 300) {
    error = "HTTP " + std::to_string(status);
    return {};
  }
  return body;
}

Remote parse_latest_release(const std::string& json, const char* package)
{
  Remote r;
  if (!key_string(json, 0, "tag_name", r.tag)) {
    r.error = "No tag_name";
    return r;
  }
  r.upstream = strip_v(r.tag);
  r.debian = r.upstream + "-1";

  std::size_t apos = 0;
  if (!find_key(json, 0, "assets", apos) || apos >= json.size() || json[apos] != '[') {
    r.error = "No assets";
    return r;
  }
  ++apos;
  while (apos < json.size()) {
    skip_ws(json, apos);
    if (apos >= json.size())
      break;
    if (json[apos] == ']')
      break;
    if (json[apos] == ',') {
      ++apos;
      continue;
    }
    if (json[apos] != '{')
      break;
    const std::size_t end = match_brace(json, apos);
    if (end == std::string::npos)
      break;
    const std::string obj = json.substr(apos, end - apos + 1);
    std::string name;
    if (key_string(obj, 0, "name", name) && is_deb_name(name, package)) {
      key_string(obj, 0, "browser_download_url", r.url);
      key_string(obj, 0, "digest", r.digest);
      key_long(obj, 0, "size", r.size);
      r.ok = !r.url.empty();
      if (!r.ok)
        r.error = "Deb asset has no URL";
      return r;
    }
    apos = end + 1;
  }
  r.error = "No amd64 .deb asset";
  return r;
}

Remote fetch_latest(const char* owner, const char* repo, const char* package)
{
  Remote r;
  if (!owner || !repo || !package) {
    r.error = "Bad catalog row";
    return r;
  }
  const std::string url =
      std::string("https://api.github.com/repos/") + owner + "/" + repo + "/releases/latest";
  std::string err;
  const std::string body = http_get(url, err);
  if (body.empty()) {
    if (err == "HTTP 404") {
      r.no_release = true;
      return r;
    }
    if (err == "HTTP 403" || err == "HTTP 429") {
      r.rate_limited = true;
      r.error = err;
      return r;
    }
    r.error = err.empty() ? "Empty response" : err;
    return r;
  }
  return parse_latest_release(body, package);
}

}  // namespace sideboard
