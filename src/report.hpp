// The result of checking one recording folder, and the two ways to print it.
#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

#include "rules.hpp"

namespace rosbagcheck {

enum class Status { Pass, Fail, Skip };

// The outcome of one check. A check that never ran stays Skip, so it can never count as passed.
struct CheckResult {
  Status status = Status::Skip;
  std::string summary = "not checked";
  std::vector<std::string> problems;

  void fail(const std::string& problem);
};

// One row of the topic table: what was measured, the rule (if any) and the result.
struct TopicReport {
  std::string topic;
  std::uint64_t messages = 0;
  double rate_hz = 0.0;
  double max_gap_s = 0.0;
  double max_gap_from_s = 0.0;  // where the longest gap starts, seconds after recording start
  std::optional<TopicRule> rule;
  Status status = Status::Skip;  // Skip = no rule for this topic, so it is not judged
};

struct RecordingReport {
  std::string path;
  CheckResult copy;     // check 1: complete copy
  CheckResult closed;   // check 2: properly closed
  CheckResult sensors;  // check 3: sensors alive
  std::vector<TopicReport> topics;

  // Healthy only if all three checks passed.
  bool healthy() const;
};

std::string render_text(const RecordingReport& report);
nlohmann::json to_json(const RecordingReport& report);

}  // namespace rosbagcheck
