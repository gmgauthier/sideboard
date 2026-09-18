/* SPDX-License-Identifier: Unlicense */

#include "catalog.hpp"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

namespace {

const long kMaxDeb = 50L * 1024L * 1024L;
const int kTimeoutSec = 300;
const char* kCacheDir = "/var/cache/sideboard";

void die(int code, const char* msg)
{
  std::fprintf(stdout, "ERR %s\n", msg);
  std::fflush(stdout);
  std::_Exit(code);
}

bool ends_with(const char* s, const char* suf)
{
  const std::size_t n = std::strlen(s);
  const std::size_t m = std::strlen(suf);
  return n >= m && std::strcmp(s + n - m, suf) == 0;
}

const char* base_name(const char* path)
{
  const char* slash = std::strrchr(path, '/');
  return slash ? slash + 1 : path;
}

bool valid_deb_name(const char* name)
{
  /* pkg_version_amd64.deb — no slashes, no "..". */
  if (!name || name[0] == '\0' || name[0] == '.')
    return false;
  if (std::strchr(name, '/') || std::strstr(name, ".."))
    return false;
  return ends_with(name, "_amd64.deb");
}

bool copy_file(const char* src, const char* dest)
{
  const int in = ::open(src, O_RDONLY | O_CLOEXEC);
  if (in < 0)
    return false;
  const int out = ::open(dest, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
  if (out < 0) {
    ::close(in);
    return false;
  }
  char buf[64 * 1024];
  for (;;) {
    const ssize_t n = ::read(in, buf, sizeof(buf));
    if (n < 0) {
      ::close(in);
      ::close(out);
      return false;
    }
    if (n == 0)
      break;
    ssize_t off = 0;
    while (off < n) {
      const ssize_t w = ::write(out, buf + off, static_cast<size_t>(n - off));
      if (w <= 0) {
        ::close(in);
        ::close(out);
        return false;
      }
      off += w;
    }
  }
  ::close(in);
  if (::close(out) != 0)
    return false;
  return true;
}

std::string first_err_line(const std::string& blob)
{
  std::string line;
  std::string first;
  std::string apt_e;
  for (char c : blob) {
    if (c == '\n' || c == '\r') {
      if (!line.empty()) {
        if (first.empty())
          first = line;
        if (apt_e.empty() && (line.compare(0, 2, "E:") == 0 || line.compare(0, 4, "Err:") == 0))
          apt_e = line;
      }
      line.clear();
    } else {
      line += c;
    }
  }
  if (!line.empty() && first.empty())
    first = line;
  if (!line.empty() && apt_e.empty() &&
      (line.compare(0, 2, "E:") == 0 || line.compare(0, 4, "Err:") == 0))
    apt_e = line;
  return apt_e.empty() ? first : apt_e;
}

int run_apt(const char* op, const char* arg, std::string& output)
{
  int pipefd[2];
  if (::pipe(pipefd) != 0)
    return -1;
  const pid_t pid = ::fork();
  if (pid < 0) {
    ::close(pipefd[0]);
    ::close(pipefd[1]);
    return -1;
  }
  if (pid == 0) {
    ::close(pipefd[0]);
    ::dup2(pipefd[1], STDOUT_FILENO);
    ::dup2(pipefd[1], STDERR_FILENO);
    if (pipefd[1] != STDOUT_FILENO && pipefd[1] != STDERR_FILENO)
      ::close(pipefd[1]);
    ::setenv("DEBIAN_FRONTEND", "noninteractive", 1);
    ::setenv("LANG", "C.UTF-8", 1);
    ::setenv("LC_ALL", "C.UTF-8", 1);
    char* const argv[] = {const_cast<char*>("/usr/bin/apt-get"), const_cast<char*>(op),
                          const_cast<char*>("-y"), const_cast<char*>(arg), nullptr};
    ::execv("/usr/bin/apt-get", argv);
    ::_exit(127);
  }
  ::close(pipefd[1]);
  output.clear();
  const time_t start = ::time(nullptr);
  char buf[4096];
  for (;;) {
    if (::time(nullptr) - start > kTimeoutSec) {
      ::kill(pid, SIGTERM);
      ::sleep(1);
      ::kill(pid, SIGKILL);
      ::close(pipefd[0]);
      int st = 0;
      ::waitpid(pid, &st, 0);
      output += "timeout";
      return -1;
    }
    struct pollfd pfd;
    pfd.fd = pipefd[0];
    pfd.events = POLLIN;
    const int pr = ::poll(&pfd, 1, 500);
    if (pr > 0 && (pfd.revents & POLLIN)) {
      const ssize_t n = ::read(pipefd[0], buf, sizeof(buf));
      if (n > 0)
        output.append(buf, static_cast<size_t>(n));
      else if (n == 0)
        break;
    }
    int st = 0;
    const pid_t w = ::waitpid(pid, &st, WNOHANG);
    if (w == pid) {
      ssize_t nread = 0;
      while ((nread = ::read(pipefd[0], buf, sizeof(buf))) > 0)
        output.append(buf, static_cast<size_t>(nread));
      ::close(pipefd[0]);
      if (WIFEXITED(st))
        return WEXITSTATUS(st);
      return -1;
    }
  }
  ::close(pipefd[0]);
  int st = 0;
  ::waitpid(pid, &st, 0);
  if (WIFEXITED(st))
    return WEXITSTATUS(st);
  return -1;
}

bool catalog_package(const char* pkg)
{
  std::size_t n = 0;
  const sideboard::App* apps = sideboard::catalog(&n);
  for (std::size_t i = 0; i < n; ++i) {
    if (std::strcmp(apps[i].package, pkg) == 0)
      return true;
  }
  return false;
}

int finish_apt(int rc, const std::string& output)
{
  if (rc == 0) {
    std::fprintf(stdout, "OK\n");
    std::fflush(stdout);
    return 0;
  }
  const std::string line = first_err_line(output);
  die(1, line.empty() ? "apt-get failed" : line.c_str());
  return 1;
}

}  // namespace

int main(int argc, char** argv)
{
  if (argc != 3)
    die(2, "usage: sideboard-helper install /absolute/path.deb | remove package");

  if (std::strcmp(argv[1], "remove") == 0) {
    if (!catalog_package(argv[2]))
      die(2, "package is not in the Sideboard catalog");
    std::string output;
    return finish_apt(run_apt("remove", argv[2], output), output);
  }

  if (std::strcmp(argv[1], "install") != 0)
    die(2, "usage: sideboard-helper install /absolute/path.deb | remove package");

  const char* src = argv[2];
  if (!src || src[0] != '/')
    die(2, "path must be absolute");
  if (std::strstr(src, ".."))
    die(2, "path must not contain ..");
  if (!ends_with(src, ".deb"))
    die(2, "path must end in .deb");

  const char* name = base_name(src);
  if (!valid_deb_name(name))
    die(2, "refusing unexpected .deb name");

  struct stat st;
  if (::stat(src, &st) != 0 || !S_ISREG(st.st_mode))
    die(2, "not a regular file");
  if (st.st_size <= 0 || st.st_size > kMaxDeb)
    die(2, "file too large");

  if (::mkdir(kCacheDir, 0755) != 0 && errno != EEXIST)
    die(1, "cannot create /var/cache/sideboard");

  std::string dest = std::string(kCacheDir) + "/" + name;
  if (!copy_file(src, dest.c_str()))
    die(1, "cannot copy into /var/cache/sideboard");

  std::string output;
  return finish_apt(run_apt("install", dest.c_str(), output), output);
}
