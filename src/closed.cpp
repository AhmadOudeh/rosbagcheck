#include "closed.hpp"

#include <algorithm>
#include <mcap/reader.hpp>

namespace rosbagcheck {

namespace fs = std::filesystem;

std::vector<fs::path> list_recordings(const fs::path& folder) {
  std::vector<fs::path> recordings;
  for (const fs::directory_entry& item : fs::directory_iterator(folder)) {
    if (item.is_regular_file() && item.path().extension() == ".mcap") {
      recordings.push_back(item.path());
    }
  }
  std::sort(recordings.begin(), recordings.end());
  return recordings;
}

CheckResult check_closed(const fs::path& folder) {
  CheckResult result;
  std::vector<fs::path> recordings;
  try {
    recordings = list_recordings(folder);
  } catch (const fs::filesystem_error& error) {
    result.fail(std::string("cannot list the folder: ") + error.what());
    result.summary = "folder not readable";
    return result;
  }
  if (recordings.empty()) {
    result.fail("no .mcap files in the folder");
    result.summary = "nothing to check";
    return result;
  }

  for (const fs::path& path : recordings) {
    std::string name = path.filename().string();
    mcap::McapReader reader;
    mcap::Status status = reader.open(path.string());
    if (!status.ok()) {
      result.fail(name + ": not a readable MCAP file (" + status.message + ")");
      continue;
    }
    // A recorder that was killed (robot switched off, disk full) never writes the summary and
    // footer at the end of the file. NoFallbackScan: report it, do not try to repair it.
    status = reader.readSummary(mcap::ReadSummaryMethod::NoFallbackScan);
    if (!status.ok()) {
      result.fail(name + ": not closed properly, no valid summary and footer (" + status.message +
                  ")");
    }
    reader.close();
  }

  if (result.problems.empty()) {
    result.status = Status::Pass;
    result.summary = std::to_string(recordings.size()) + " file(s) closed properly";
  } else {
    result.summary = std::to_string(result.problems.size()) + " of " +
                     std::to_string(recordings.size()) + " file(s) not closed properly";
  }
  return result;
}

}  // namespace rosbagcheck
