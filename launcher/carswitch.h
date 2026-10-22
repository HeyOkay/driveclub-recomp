// Builds the menu files that make the car switch in the car select faster.
#pragma once
#include <string>

// Reads the three showroom menu files (Canada, Chile, UK) from the player's own game archive and writes copies with
// shorter blur animations to `out_dir`. The runtime serves them in place of the originals; the game folders are never
// touched. Skips the work when the copies in `out_dir` already match the archive. Returns "" or a short description of
// a problem.
std::wstring BuildFastCarSwitch(const std::wstring& update_dir, const std::wstring& base_dir, const std::wstring& out_dir);
