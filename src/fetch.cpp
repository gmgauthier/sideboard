/* SPDX-License-Identifier: Unlicense */

#include "fetch.hpp"
#include "config.hpp"

#include <curl/curl.h>
#include <openssl/evp.h>

#include <cctype>
#include <cstdio>
#include <fstream>
#include <vector>

namespace sideboard {
namespace {

const long kMaxDeb = 50L * 1024L * 1024L;

struct DlState {
  FILE* fp = nullptr;
  long written = 0;
  DownloadProgress progress;
};

size_t file_write(char* ptr, size_t size, size_t nmemb, void* userdata)
{
  auto* st = static_cast<DlState*>(userdata);
  const size_t n = size * nmemb;
  if (st->written + static_cast<long>(n) > kMaxDeb)
    return 0;
  const size_t w = std::fwrite(ptr, 1, n, st->fp);
  st->written += static_cast<long>(w);
  return w;
}

int xfer(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t)
{
  auto* st = static_cast<DlState*>(clientp);
  if (st->progress)
    st->progress(static_cast<long>(dlnow), static_cast<long>(dltotal));
  return 0;
}

std::string to_hex(const unsigned char* p, unsigned int n)
{
  static const char* kDigits = "0123456789abcdef";
  std::string out;
  out.resize(n * 2);
  for (unsigned int i = 0; i < n; ++i) {
    out[i * 2] = kDigits[(p[i] >> 4) & 0xf];
    out[i * 2 + 1] = kDigits[p[i] & 0xf];
  }
  return out;
}

}  // namespace

bool download_file(const std::string& url, const std::string& dest, std::string& error,
                   const DownloadProgress& progress)
{
  error.clear();
  FILE* fp = std::fopen(dest.c_str(), "wb");
  if (!fp) {
    error = "Cannot write " + dest;
    return false;
  }
  CURL* curl = curl_easy_init();
  if (!curl) {
    std::fclose(fp);
    error = "curl init failed";
    return false;
  }
  DlState st;
  st.fp = fp;
  st.progress = progress;
  const std::string ua =
      std::string("sideboard/") + VERSION + " (+https://github.com/gmgauthier/sideboard)";
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_USERAGENT, ua.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, file_write);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &st);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
  curl_easy_setopt(curl, CURLOPT_MAXFILESIZE, kMaxDeb);
  curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https,http");
  curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xfer);
  curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &st);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
  const CURLcode rc = curl_easy_perform(curl);
  long status = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
  curl_easy_cleanup(curl);
  std::fclose(fp);
  if (rc != CURLE_OK) {
    error = curl_easy_strerror(rc);
    std::remove(dest.c_str());
    return false;
  }
  if (status < 200 || status >= 300) {
    error = "HTTP " + std::to_string(status);
    std::remove(dest.c_str());
    return false;
  }
  return true;
}

std::string sha256_file(const std::string& path, std::string& error)
{
  error.clear();
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    error = "Cannot read " + path;
    return {};
  }
  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  if (!ctx || EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
    if (ctx)
      EVP_MD_CTX_free(ctx);
    error = "SHA-256 init failed";
    return {};
  }
  std::vector<char> buf(64 * 1024);
  while (in) {
    in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
    const auto n = in.gcount();
    if (n > 0 && EVP_DigestUpdate(ctx, buf.data(), static_cast<size_t>(n)) != 1) {
      EVP_MD_CTX_free(ctx);
      error = "SHA-256 update failed";
      return {};
    }
  }
  unsigned char out[EVP_MAX_MD_SIZE];
  unsigned int len = 0;
  if (EVP_DigestFinal_ex(ctx, out, &len) != 1) {
    EVP_MD_CTX_free(ctx);
    error = "SHA-256 final failed";
    return {};
  }
  EVP_MD_CTX_free(ctx);
  return to_hex(out, len);
}

bool digest_matches(const std::string& digest, const std::string& hex)
{
  std::string want = digest;
  const std::string prefix = "sha256:";
  if (want.compare(0, prefix.size(), prefix) == 0)
    want = want.substr(prefix.size());
  if (want.size() != hex.size())
    return false;
  for (std::size_t i = 0; i < want.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(want[i])) !=
        std::tolower(static_cast<unsigned char>(hex[i])))
      return false;
  }
  return true;
}

}  // namespace sideboard
