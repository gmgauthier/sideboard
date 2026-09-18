/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <cstddef>

namespace sideboard {

struct App {
  const char* id;
  const char* display;
  const char* package;
  const char* owner;
  const char* repo;
  const char* summary;
};

const App* catalog(std::size_t* count);

}  // namespace sideboard
