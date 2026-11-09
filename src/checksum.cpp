#include "checksum.hpp"

#include <openssl/evp.h>

#include <cctype>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>

namespace rosbagcheck {

namespace fs = std::filesystem;

namespace {

bool is_hex_hash(const std::string& text) {
  if (text.size() != 64) return false;
  for (char c : text) {
    if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
  }
  return true;
}

// A name from SHA256SUMS must point inside the recording folder.
bool is_inside_folder(const fs::path& name) {
  return name.is_relative() && *name.begin() != "..";
}

}  // namespace

ParsedSums parse_sums(const std::string& text) {
  ParsedSums result;
  std::istringstream lines(text);
  std::string line;
  int number = 0;
  while (std::getline(lines, line)) {
    number++;
    if (!line.empty() && line.back() == '\r') line.pop_back();  // file written on Windows
    if (line.empty()) continue;
    std::string where = "SHA256SUMS line " + std::to_string(number) + ": ";

    // "<64 hex digits><space><space or *><name>"
    bool format_ok = line.size() > 66 && is_hex_hash(line.substr(0, 64)) && line[64] == ' ' &&
                     (line[65] == ' ' || line[65] == '*');
    if (!format_ok) {
      result.problems.push_back(where + "not in sha256sum format");
      continue;
    }
    // lexically_normal: "./a.mcap" -> "a.mcap", "sub/../../a.mcap" -> "../a.mcap"
    fs::path name = fs::path(line.substr(66)).lexically_normal();
    if (!is_inside_folder(name)) {
      result.problems.push_back(where + "file name points outside the folder");
      continue;
    }
    std::string hash = line.substr(0, 64);
    for (char& c : hash) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    result.entries.push_back({hash, name.string()});
  }
  return result;
}

std::string sha256_hex(std::istream& in) {
  EVP_MD_CTX* context = EVP_MD_CTX_new();
  EVP_DigestInit_ex(context, EVP_sha256(), nullptr);

  // Read 64 KiB at a time, so memory use does not grow with the file size (NFR-2).
  std::vector<char> block(64 * 1024);
  while (in.read(block.data(), static_cast<std::streamsize>(block.size())) || in.gcount() > 0) {
    EVP_DigestUpdate(context, block.data(), static_cast<std::size_t>(in.gcount()));
  }
  unsigned char digest[32];
  EVP_DigestFinal_ex(context, digest, nullptr);
  EVP_MD_CTX_free(context);
  if (in.bad()) throw std::runtime_error("read error");

  const char* digits = "0123456789abcdef";
  std::string hex;
  for (unsigned char byte : digest) {
    hex += digits[byte / 16];
    hex += digits[byte % 16];
  }
  return hex;
}

CheckResult check_copy(const fs::path& folder) {
  CheckResult result;
  fs::path sums_path = folder / "SHA256SUMS";
  std::ifstream sums_file(sums_path);
  if (!sums_file) {
    result.fail(fs::exists(sums_path) ? "SHA256SUMS cannot be read"
                                      : "SHA256SUMS missing: cannot prove the copy is complete");
    result.summary = "no SHA256SUMS";
    return result;
  }
  std::stringstream text;
  text << sums_file.rdbuf();
  ParsedSums sums = parse_sums(text.str());
  for (const std::string& problem : sums.problems) result.fail(problem);
  if (sums.entries.empty() && sums.problems.empty()) result.fail("SHA256SUMS lists no files");

  // Every listed file exists and has the listed checksum.
  std::set<std::string> listed;
  for (const SumsEntry& entry : sums.entries) {
    listed.insert(entry.name);
    std::ifstream file(folder / entry.name, std::ios::binary);
    if (!fs::is_regular_file(folder / entry.name)) {
      result.fail(entry.name + ": missing");
    } else if (!file) {
      result.fail(entry.name + ": cannot be read");
    } else {
      try {
        if (sha256_hex(file) != entry.hash) {
          result.fail(entry.name + ": checksum differs (copy incomplete or file changed)");
        }
      } catch (const std::exception& error) {
        result.fail(entry.name + ": " + error.what());
      }
    }
  }

  // A recording that nobody listed cannot be trusted either.
  try {
    for (const fs::directory_entry& item : fs::directory_iterator(folder)) {
      std::string name = item.path().filename().string();
      if (item.path().extension() == ".mcap" && listed.count(name) == 0) {
        result.fail(name + ": not listed in SHA256SUMS");
      }
    }
  } catch (const fs::filesystem_error& error) {
    result.fail(std::string("cannot list the folder: ") + error.what());
  }

  if (result.problems.empty()) {
    result.status = Status::Pass;
    result.summary = std::to_string(sums.entries.size()) + " file(s) match SHA256SUMS";
  } else {
    result.summary = std::to_string(result.problems.size()) + " problem(s)";
  }
  return result;
}

}  // namespace rosbagcheck
