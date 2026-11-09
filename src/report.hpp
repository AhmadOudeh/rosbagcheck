// The result of checking one recording folder, and the two ways to print it.
#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace rosbagcheck {

enum class Status { Pass, Fail, Skip };

// The outcome of one check. A check that never ran stays Skip, so it can never count as passed.
struct CheckResult {
  Status status = Status::Skip;
  std::string summary = "not checked";
  std::vector<std::string> problems;

  void fail(const std::string& problem);
};

struct RecordingReport {
  std::string path;
  CheckResult copy;     // check 1: complete copy
  CheckResult closed;   // check 2: properly closed
  CheckResult sensors;  // check 3: sensors alive

  // Healthy only if all three checks passed.
  bool healthy() const;
};

std::string render_text(const RecordingReport& report);
nlohmann::json to_json(const RecordingReport& report);

}  // namespace rosbagcheck
