/* SPDX-License-Identifier: Unlicense */

#include "fetch.hpp"
#include "remote.hpp"
#include "check.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>

namespace {

/* Answers one request with headers and a few bytes, then stalls for up to STALL_MS. */
class StallServer {
 public:
  explicit StallServer(int stall_ms)
  {
    fd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    ::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    ::listen(fd_, 1);
    socklen_t len = sizeof(addr);
    ::getsockname(fd_, reinterpret_cast<sockaddr*>(&addr), &len);
    port_ = ntohs(addr.sin_port);
    thread_ = std::thread([this, stall_ms]() { serve(stall_ms); });
  }

  ~StallServer()
  {
    done_ = true;
    thread_.join();
    ::close(fd_);
  }

  std::string url() const
  {
    return "http://127.0.0.1:" + std::to_string(port_) + "/x_1_amd64.deb";
  }

 private:
  void serve(int stall_ms)
  {
    pollfd pfd{fd_, POLLIN, 0};
    if (::poll(&pfd, 1, 5000) <= 0)
      return;
    const int c = ::accept(fd_, nullptr, nullptr);
    if (c < 0)
      return;
    char buf[2048];
    const ssize_t got = ::read(c, buf, sizeof(buf));
    (void)got;
    const std::string head =
        "HTTP/1.1 200 OK\r\nContent-Length: 10000000\r\nContent-Type: application/json\r\n\r\nxxxx";
    const ssize_t put = ::write(c, head.data(), head.size());
    (void)put;
    const auto start = std::chrono::steady_clock::now();
    while (!done_ && std::chrono::steady_clock::now() - start < std::chrono::milliseconds(stall_ms))
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    ::close(c);
  }

  int fd_ = -1;
  int port_ = 0;
  std::atomic<bool> done_{false};
  std::thread thread_;
};

double seconds_since(std::chrono::steady_clock::time_point t)
{
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - t).count();
}

void test_download_honours_cancel()
{
  StallServer server(6000);
  char tmpl[] = "/tmp/sideboard-fetch-XXXXXX";
  const char* dir = ::mkdtemp(tmpl);
  CHECK(dir != nullptr);
  const std::string dest = std::string(dir ? dir : "/tmp") + "/x_1_amd64.deb";

  std::atomic<bool> cancel{false};
  std::thread canceller([&cancel]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    cancel = true;
  });
  const auto start = std::chrono::steady_clock::now();
  std::string err;
  const bool ok = sideboard::download_file(server.url(), dest, err, {}, &cancel);
  const double took = seconds_since(start);
  canceller.join();

  CHECK(!ok);
  CHECK(took < 3.0);
  struct stat st;
  CHECK(::stat(dest.c_str(), &st) != 0);
  if (dir)
    ::rmdir(dir);
}

void test_http_get_honours_cancel()
{
  StallServer server(6000);
  std::atomic<bool> cancel{false};
  std::thread canceller([&cancel]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    cancel = true;
  });
  const auto start = std::chrono::steady_clock::now();
  std::string err;
  const std::string body = sideboard::http_get(server.url(), err, &cancel);
  const double took = seconds_since(start);
  canceller.join();

  CHECK(body.empty());
  CHECK(!err.empty());
  CHECK(took < 3.0);
}

void test_cancel_already_set()
{
  std::atomic<bool> cancel{true};
  const auto start = std::chrono::steady_clock::now();
  std::string err;
  const sideboard::Remote r = sideboard::fetch_latest("o", "r", "p", &cancel);
  CHECK(!r.ok);
  CHECK(seconds_since(start) < 3.0);
}

}  // namespace

int main()
{
  test_download_honours_cancel();
  test_http_get_honours_cancel();
  test_cancel_already_set();
  return suite_test::done("fetch");
}
