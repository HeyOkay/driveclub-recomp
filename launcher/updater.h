// Looks for a newer release on the project's Releases page and installs it over the program files.
#pragma once
#include <functional>
#include <string>

namespace Updater {

struct Release {
  std::wstring version;   // "0.3.1"
  std::wstring zip_url;   // the win64 zip
  std::wstring sums_url;  // SHA256SUMS.txt
};

// "0.3.1" newer than "0.3.0": compares the numbers of the dotted parts.
bool IsNewer(const std::wstring& candidate, const std::wstring& current);

// Reads the answer of the releases/latest call. False when it holds no release, or no zip and checksum file.
bool ParseRelease(const std::string& json, Release& out);

// The newest published release when it is newer than `current`. False when it is not, when the player is
// offline, or when anything else goes wrong (`why` says what, for the log).
bool CheckNewer(const std::wstring& current, Release& out, std::string& why);

// Downloads the zip, checks it against the checksum file, unpacks it into <home>\update and starts the script
// that copies it over the program files once the launcher has closed. The player's data, saves, mods, logs and
// launcher.ini are not touched. True: the caller must close the launcher now. `progress` gets 0..100.
bool Install(const Release& release, const std::wstring& home, const std::function<void(int)>& progress,
             std::wstring& error);

// Removes the leftovers of an earlier update (the folder is in use while the script still runs).
void CleanUp(const std::wstring& home);

}  // namespace Updater
