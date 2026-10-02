/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <functional>
#include <string>
#include <vector>

namespace sideboard {
namespace helper {

/* Runs apt-get OP -y ARG, filling OUTPUT. Returns the exit status, or -1. */
using AptRunner = std::function<int(const char* op, const std::string& arg, std::string& output)>;

struct Result {
  int code = 0;        /* process exit code: 0 ok, 1 failure, 2 refused */
  std::string message; /* "OK" or the error line */
};

/* Runs ARGV (absolute path first) with stdout and stderr captured. -1 on spawn error or timeout. */
int run_capture(const std::vector<std::string>& argv, int timeout_sec, std::string& output);
int run_apt(const char* op, const std::string& arg, std::string& output);

bool catalog_package(const char* pkg);
/* The Package: field of a .deb (via dpkg-deb), or empty when it cannot be read. */
std::string deb_package(const std::string& deb);
std::string first_err_line(const std::string& blob);

Result install_deb(const char* src, const std::string& cache_dir, const AptRunner& apt);
Result remove_package(const char* pkg, const AptRunner& apt);

}  // namespace helper
}  // namespace sideboard
