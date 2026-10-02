/* SPDX-License-Identifier: Unlicense */

#include "helper_core.hpp"
#include "catalog.hpp"

#include <fcntl.h>
#include <glib.h>
#include <poll.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace sideboard {
namespace helper {
namespace {

const long kMaxDeb = 50L * 1024L * 1024L;
const int kTimeoutSec = 300;

Result refuse(const char* msg)
{
  return Result{2, msg};
}

Result fail(const std::string& msg)
{
  return Result{1, msg};
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

/* Copies from an open descriptor into DEST, readable only by its owner. */
bool copy_fd(int in, const char* dest)
{
  const int out = ::open(dest, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC, 0600);
  if (out < 0)
    return false;
  bool ok = ::fchmod(out, 0600) == 0;
  char buf[64 * 1024];
  while (ok) {
    const ssize_t n = ::read(in, buf, sizeof(buf));
    if (n < 0) {
      ok = false;
      break;
    }
    if (n == 0)
      break;
    ssize_t off = 0;
    while (off < n) {
      const ssize_t w = ::write(out, buf + off, static_cast<size_t>(n - off));
      if (w <= 0) {
        ok = false;
        break;
      }
      off += w;
    }
  }
  if (::close(out) != 0)
    ok = false;
  if (!ok)
    ::unlink(dest);
  return ok;
}

/* Creates DIR as 0700, or tightens an existing real directory to 0700. */
bool private_dir(const std::string& dir)
{
  if (::mkdir(dir.c_str(), 0700) != 0 && errno != EEXIST)
    return false;
  struct stat st;
  if (::lstat(dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode))
    return false;
  if ((st.st_mode & 07777) != 0700 && ::chmod(dir.c_str(), 0700) != 0)
    return false;
  return true;
}

/* "sha256:" plus 64 hex digits, returned lower-case without the prefix; empty if malformed. */
std::string digest_hex(const std::string& digest)
{
  const std::string prefix = "sha256:";
  if (digest.compare(0, prefix.size(), prefix) != 0 || digest.size() != prefix.size() + 64)
    return {};
  std::string hex = digest.substr(prefix.size());
  for (auto& c : hex) {
    if (!std::isxdigit(static_cast<unsigned char>(c)))
      return {};
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return hex;
}

/* SHA-256 of PATH as lower-case hex, or empty on a read error. */
std::string sha256_path(const std::string& path)
{
  const int fd = ::open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
  if (fd < 0)
    return {};
  GChecksum* sum = g_checksum_new(G_CHECKSUM_SHA256);
  bool ok = true;
  unsigned char buf[64 * 1024];
  for (;;) {
    const ssize_t n = ::read(fd, buf, sizeof(buf));
    if (n < 0) {
      ok = false;
      break;
    }
    if (n == 0)
      break;
    g_checksum_update(sum, buf, n);
  }
  ::close(fd);
  std::string hex = ok ? g_checksum_get_string(sum) : "";
  g_checksum_free(sum);
  return hex;
}

Result finish_apt(int rc, const std::string& output)
{
  if (rc == 0)
    return Result{0, "OK"};
  const std::string line = first_err_line(output);
  return fail(line.empty() ? "apt-get failed" : line);
}

}  // namespace

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

int run_capture(const std::vector<std::string>& args, int timeout_sec, std::string& output)
{
  output.clear();
  if (args.empty())
    return -1;
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
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& a : args)
      argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);
    ::execv(argv[0], argv.data());
    ::_exit(127);
  }
  ::close(pipefd[1]);
  const time_t start = ::time(nullptr);
  char buf[4096];
  for (;;) {
    if (::time(nullptr) - start > timeout_sec) {
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

int run_apt(const char* op, const std::string& arg, std::string& output)
{
  return run_capture({"/usr/bin/apt-get", op, "-y", arg}, kTimeoutSec, output);
}

std::string deb_package(const std::string& deb)
{
  std::string out;
  if (run_capture({"/usr/bin/dpkg-deb", "--field", deb, "Package"}, 60, out) != 0)
    return {};
  while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' '))
    out.pop_back();
  if (out.empty() || out.find_first_of(" \t\n/") != std::string::npos)
    return {};
  return out;
}

bool catalog_package(const char* pkg)
{
  std::size_t n = 0;
  const App* apps = catalog(&n);
  for (std::size_t i = 0; i < n; ++i) {
    if (std::strcmp(apps[i].package, pkg) == 0)
      return true;
  }
  return false;
}

Result remove_package(const char* pkg, const AptRunner& apt)
{
  if (!catalog_package(pkg))
    return refuse("package is not in the Sideboard catalog");
  std::string output;
  return finish_apt(apt("remove", pkg, output), output);
}

Result install_deb(const char* src, const std::string& cache_dir, const AptRunner& apt,
                   const std::string& digest)
{
  if (!src || src[0] != '/')
    return refuse("path must be absolute");
  if (std::strstr(src, ".."))
    return refuse("path must not contain ..");
  if (!ends_with(src, ".deb"))
    return refuse("path must end in .deb");

  const char* name = base_name(src);
  if (!valid_deb_name(name))
    return refuse("refusing unexpected .deb name");

  std::string want;
  if (!digest.empty()) {
    want = digest_hex(digest);
    if (want.empty())
      return refuse("digest must be sha256: and 64 hex digits");
  }

  /* Never follow a symlink, and check the file that was actually opened. */
  const int in = ::open(src, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
  if (in < 0)
    return refuse("not a regular file");
  struct stat st;
  if (::fstat(in, &st) != 0 || !S_ISREG(st.st_mode)) {
    ::close(in);
    return refuse("not a regular file");
  }
  if (st.st_size <= 0 || st.st_size > kMaxDeb) {
    ::close(in);
    return refuse("file too large");
  }

  if (!private_dir(cache_dir)) {
    ::close(in);
    return fail("cannot create " + cache_dir);
  }

  const std::string dest = cache_dir + "/" + name;
  const bool copied = copy_fd(in, dest.c_str());
  ::close(in);
  if (!copied)
    return fail("cannot copy into " + cache_dir);

  /* Read the package name from root's copy, not from the file name the caller chose. */
  const std::string package = deb_package(dest);
  if (package.empty() || !catalog_package(package.c_str())) {
    ::unlink(dest.c_str());
    return refuse("package is not in the Sideboard catalog");
  }

  /* Check the published digest against root's copy, which the caller can no longer change. */
  if (!want.empty() && sha256_path(dest) != want) {
    ::unlink(dest.c_str());
    return refuse("SHA-256 mismatch. The file was not installed.");
  }

  std::string output;
  const int rc = apt("install", dest, output);
  if (rc != 0)
    ::unlink(dest.c_str());
  return finish_apt(rc, output);
}

}  // namespace helper
}  // namespace sideboard
