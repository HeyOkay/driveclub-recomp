// Builds only display-name metadata from the player's own archive; no game data
// ships with the overlay.
#pragma once
#include "datx.h"

namespace SceneTrackNames {
inline std::vector<std::string> Fields(const std::string &line) {
  std::vector<std::string> fields;
  std::string value;
  bool quoted = false;
  for (size_t i = 0; i < line.size(); ++i) {
    const char c = line[i];
    if (c == '"') {
      if (quoted && i + 1 < line.size() && line[i + 1] == '"') {
        value += '"';
        ++i;
      } else
        quoted = !quoted;
    } else if (c == ',' && !quoted) {
      fields.push_back(value);
      value.clear();
    } else if (c != '\r')
      value += c;
  }
  fields.push_back(value);
  return fields;
}
inline std::string Utf8(const std::string &input) {
  // This game CSV uses Windows-1252 (for example the accent in Salar de
  // Surire's region).
  const int n =
      MultiByteToWideChar(1252, 0, input.data(), int(input.size()), nullptr, 0);
  std::wstring wide(size_t(n), L'\0');
  MultiByteToWideChar(1252, 0, input.data(), int(input.size()), wide.data(), n);
  const int bytes = WideCharToMultiByte(CP_UTF8, 0, wide.data(), n, nullptr, 0,
                                        nullptr, nullptr);
  std::string output(size_t(bytes), '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.data(), n, output.data(), bytes, nullptr,
                      nullptr);
  return output;
}
inline bool Write(const std::wstring &update, const std::wstring &base,
                  const std::wstring &path) {
  std::vector<uint8_t> csv;
  if (!DatxExtract(update, base, "newdata/tracks.csv", csv).empty())
    return false;
  std::string text(csv.begin(), csv.end()), output;
  size_t start = 0, id = size_t(-1), title = size_t(-1);
  while (start < text.size()) {
    const size_t end = text.find('\n', start);
    const auto fields = Fields(
        text.substr(start, end == std::string::npos ? end : end - start));
    if (start == 0) {
      for (size_t i = 0; i < fields.size(); ++i) {
        if (fields[i] == "Track Name")
          id = i;
        if (fields[i] == "PRODUCT_NAME_1")
          title = i;
      }
      if (id == size_t(-1) || title == size_t(-1))
        return false;
    } else if (id < fields.size() && title < fields.size() &&
               !fields[id].empty() && !fields[title].empty()) {
      // Tabs/newlines would corrupt the small mapping format; fall back to the
      // internal ID.
      if (fields[id].find_first_of("\t\r\n") == std::string::npos &&
          fields[title].find_first_of("\t\r\n") == std::string::npos)
        output += fields[id] + '\t' + Utf8(fields[title]) + '\n';
    }
    if (end == std::string::npos)
      break;
    start = end + 1;
  }
  if (output.empty())
    return false;
  FILE *file = _wfopen(path.c_str(), L"wb");
  if (!file)
    return false;
  const bool written =
      std::fwrite(output.data(), 1, output.size(), file) == output.size();
  return std::fclose(file) == 0 && written;
}
} // namespace SceneTrackNames
