// The rules file (FR-4): which topics must be there, how often, and how long they may be silent.
#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace rosbagcheck {

// One rule from the rules file. No limit given = that limit is not checked.
struct TopicRule {
  std::string topic;
  std::optional<double> min_rate_hz;
  std::optional<double> max_gap_s;
  bool required = true;
};

// Any problem in the rules file. main prints it and exits with code 2.
struct RulesError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// Parses YAML text such as:
//   topics:
//     - topic: /camera/image_raw
//       min_rate_hz: 15
//       max_gap_s: 0.5
//     - topic: /diagnostics
//       required: false
std::vector<TopicRule> parse_rules(const std::string& yaml_text);

// Reads the file, then parse_rules.
std::vector<TopicRule> load_rules(const std::string& path);

}  // namespace rosbagcheck
