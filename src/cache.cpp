/* SPDX-License-Identifier: Unlicense */

#include "cache.hpp"

#include <glib.h>
#include <glibmm.h>

#include <cctype>
#include <fstream>
#include <sstream>

namespace sideboard {
namespace {

std::string json_escape(const std::string& in)
{
  std::string out;
  out.reserve(in.size());
  for (char c : in) {
    if (c == '\\' || c == '"') {
      out += '\\';
      out += c;
    } else if (c == '\n') {
      out += "\\n";
    } else {
      out += c;
    }
  }
  return out;
}

std::string json_string_field(const std::string& obj, const char* key)
{
  const std::string pat = std::string("\"") + key + "\"";
  auto p = obj.find(pat);
  if (p == std::string::npos)
    return {};
  p += pat.size();
  while (p < obj.size() && (obj[p] == ' ' || obj[p] == '\t' || obj[p] == '\n' || obj[p] == ':')) {
    if (obj[p] == ':') {
      ++p;
      break;
    }
    ++p;
  }
  while (p < obj.size() && (obj[p] == ' ' || obj[p] == '\t'))
    ++p;
  if (p >= obj.size() || obj[p] != '"')
    return {};
  ++p;
  std::string out;
  while (p < obj.size()) {
    const char c = obj[p++];
    if (c == '"')
      return out;
    if (c == '\\' && p < obj.size())
      out += obj[p++];
    else
      out += c;
  }
  return out;
}

long json_long_field(const std::string& obj, const char* key)
{
  const std::string pat = std::string("\"") + key + "\"";
  auto p = obj.find(pat);
  if (p == std::string::npos)
    return 0;
  p = obj.find(':', p);
  if (p == std::string::npos)
    return 0;
  ++p;
  while (p < obj.size() && (obj[p] == ' ' || obj[p] == '\t'))
    ++p;
  long v = 0;
  while (p < obj.size() && std::isdigit(static_cast<unsigned char>(obj[p]))) {
    v = v * 10 + (obj[p] - '0');
    ++p;
  }
  return v;
}

}  // namespace

std::string cache_path()
{
  const std::string dir = Glib::build_filename(Glib::get_user_cache_dir(), "sideboard");
  g_mkdir_with_parents(dir.c_str(), 0700);
  return Glib::build_filename(dir, "catalog.json");
}

std::string config_path()
{
  const std::string dir = Glib::build_filename(Glib::get_user_config_dir(), "sideboard");
  g_mkdir_with_parents(dir.c_str(), 0700);
  return Glib::build_filename(dir, "sideboard.ini");
}

std::string now_stamp()
{
  return Glib::DateTime::create_now_local().format("%d %b %Y, %H:%M");
}

void save_last_check(const std::string& stamp)
{
  std::ofstream out(config_path());
  if (!out)
    return;
  out << "[sideboard]\nlast_check=" << stamp << "\n";
}

std::string load_last_check()
{
  std::ifstream in(config_path());
  if (!in)
    return {};
  std::string line;
  while (std::getline(in, line)) {
    const auto eq = line.find("last_check=");
    if (eq == 0)
      return line.substr(11);
  }
  return {};
}

void save_cache(const CatalogCache& cache)
{
  std::ostringstream os;
  os << "{\n  \"checked\": \"" << json_escape(cache.checked) << "\",\n  \"apps\": {\n";
  bool first = true;
  for (const auto& kv : cache.apps) {
    const Remote& r = kv.second;
    if (!r.ok)
      continue;
    if (!first)
      os << ",\n";
    first = false;
    os << "    \"" << json_escape(kv.first) << "\": {\n";
    os << "      \"upstream\": \"" << json_escape(r.upstream) << "\",\n";
    os << "      \"debian\": \"" << json_escape(r.debian) << "\",\n";
    os << "      \"url\": \"" << json_escape(r.url) << "\",\n";
    os << "      \"digest\": \"" << json_escape(r.digest) << "\",\n";
    os << "      \"size\": " << r.size << "\n";
    os << "    }";
  }
  os << "\n  }\n}\n";
  std::ofstream out(cache_path());
  if (!out)
    return;
  out << os.str();
}

CatalogCache load_cache()
{
  CatalogCache c;
  std::ifstream in(cache_path());
  if (!in)
    return c;
  std::ostringstream os;
  os << in.rdbuf();
  const std::string json = os.str();
  c.checked = json_string_field(json, "checked");
  auto apps_at = json.find("\"apps\"");
  if (apps_at == std::string::npos)
    return c;
  auto brace = json.find('{', apps_at);
  if (brace == std::string::npos)
    return c;
  std::size_t i = brace + 1;
  while (i < json.size()) {
    while (i < json.size() && json[i] != '"' && json[i] != '}')
      ++i;
    if (i >= json.size() || json[i] == '}')
      break;
    std::string pkg;
    ++i;
    while (i < json.size() && json[i] != '"')
      pkg += json[i++];
    if (i < json.size() && json[i] == '"')
      ++i;
    auto obj_open = json.find('{', i);
    if (obj_open == std::string::npos)
      break;
    int depth = 0;
    std::size_t obj_end = obj_open;
    for (; obj_end < json.size(); ++obj_end) {
      if (json[obj_end] == '{')
        ++depth;
      else if (json[obj_end] == '}') {
        --depth;
        if (depth == 0)
          break;
      }
    }
    if (obj_end >= json.size())
      break;
    const std::string obj = json.substr(obj_open, obj_end - obj_open + 1);
    Remote r;
    r.ok = true;
    r.cached = true;
    r.upstream = json_string_field(obj, "upstream");
    r.debian = json_string_field(obj, "debian");
    r.url = json_string_field(obj, "url");
    r.digest = json_string_field(obj, "digest");
    r.size = json_long_field(obj, "size");
    if (r.debian.empty() && !r.upstream.empty())
      r.debian = r.upstream + "-1";
    if (!pkg.empty() && !r.upstream.empty())
      c.apps[pkg] = r;
    i = obj_end + 1;
  }
  return c;
}

}  // namespace sideboard
