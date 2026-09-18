/* SPDX-License-Identifier: Unlicense */

#include "catalog.hpp"

namespace sideboard {
namespace {

const App kApps[] = {
    {"earblaster", "EarBlaster", "earblaster", "gmgauthier", "earblaster",
     "Audio player in the Windows Media Player 7 shape."},
    {"readomatic", "Read-O-Matic", "readomatic", "gmgauthier", "readomatic",
     "EPUB reader in the WinHelp 4 / OS/2 VIEW.EXE shape."},
    {"yolodex", "YOLO-dex", "yolodex", "gmgauthier", "yolodex",
     "Address book in the Windows Cardfile shape."},
    {"kablamo", "Kablamo!", "kablamo", "gmgauthier", "kablamo",
     "Minesweeper for The Lunduke Computer Operating System."},
    {"ephemeris", "Ephemeris", "ephemeris", "gmgauthier", "ephemeris",
     "Calendar and day pages for LCOS."},
    {"partyline", "Partyline", "partyline", "gmgauthier", "partyline", "IRC client for LCOS."},
    {"dispatch", "Dispatch", "dispatch", "gmgauthier", "dispatch", "RSS/Atom reader for LCOS."},
};

}  // namespace

const App* catalog(std::size_t* count)
{
  if (count)
    *count = sizeof(kApps) / sizeof(kApps[0]);
  return kApps;
}

}  // namespace sideboard
