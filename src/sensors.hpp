// Check 3, "sensors alive": message count, rate and longest gap per topic, compared with the rules.
#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "report.hpp"
#include "rules.hpp"

namespace rosbagcheck {

// Log times (nanoseconds) of every message, per topic, over all files of one recording.
struct Timeline {
  std::map<std::string, std::vector<std::uint64_t>> times;
  std::uint64_t start_ns = 0;  // first message on any topic
  std::uint64_t end_ns = 0;    // last message on any topic
};

// Reads only the log times, never keeps message data. Problems go into `problems`.
Timeline read_timeline(const std::vector<std::filesystem::path>& recordings,
                       std::vector<std::string>& problems);

// Pure maths, unit tested.
// rate = (messages - 1) / (recording end - recording start): a sensor that dies early gets a low
// rate. The longest gap also counts recording start -> first message and last message -> end.
TopicReport measure(const std::string& topic, std::vector<std::uint64_t> times,
                    std::uint64_t start_ns, std::uint64_t end_ns);

// Applies one rule to one topic. Limits are inclusive: rate == min passes, gap == max passes.
Status judge(const TopicReport& topic, const TopicRule& rule, std::vector<std::string>& problems);

// Check 3 for one recording. Fills `topics` with one row per topic (sorted by name).
CheckResult check_sensors(const std::vector<std::filesystem::path>& recordings,
                          const std::vector<TopicRule>& rules, std::vector<TopicReport>& topics);

}  // namespace rosbagcheck
