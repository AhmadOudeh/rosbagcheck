// Check 1, "complete copy": every file listed in SHA256SUMS exists and has the listed checksum.
#pragma once

#include <filesystem>
#include <istream>
#include <string>
#include <vector>

#include "report.hpp"

namespace rosbagcheck {

struct SumsEntry {
  std::string hash;  // 64 lowercase hex digits
  std::string name;  // file name, relative to the recording folder
};

struct ParsedSums {
  std::vector<SumsEntry> entries;
  std::vector<std::string> problems;  // one per line that is not in sha256sum format
};

// Reads the format written by `sha256sum`: "<64 hex digits><space><space or *><file name>".
ParsedSums parse_sums(const std::string& text);

// SHA-256 of everything in the stream, as 64 lowercase hex digits.
std::string sha256_hex(std::istream& in);

CheckResult check_copy(const std::filesystem::path& folder);

}  // namespace rosbagcheck
