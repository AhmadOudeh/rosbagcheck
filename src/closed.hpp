// Check 2, "properly closed": every .mcap file has its summary and footer.
#pragma once

#include <filesystem>
#include <vector>

#include "report.hpp"

namespace rosbagcheck {

// All .mcap files directly inside the folder, sorted by name.
std::vector<std::filesystem::path> list_recordings(const std::filesystem::path& folder);

CheckResult check_closed(const std::filesystem::path& folder);

}  // namespace rosbagcheck
