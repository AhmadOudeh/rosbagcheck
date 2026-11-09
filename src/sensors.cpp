#include "sensors.hpp"

#include <algorithm>
#include <iomanip>
#include <mcap/reader.hpp>
#include <sstream>

namespace rosbagcheck {

namespace {

const double kNsPerSecond = 1e9;

// fixed(12.1, 2) = "12.10": a number with a fixed count of decimals, for the problem texts.
std::string fixed(double value, int decimals) {
  std::ostringstream text;
  text << std::fixed << std::setprecision(decimals) << value;
  return text.str();
}

// plain(0.5) = "0.5", plain(15) = "15": a limit as the user wrote it.
std::string plain(double value) {
  std::ostringstream text;
  text << value;
  return text.str();
}

}  // namespace

Timeline read_timeline(const std::vector<std::filesystem::path>& recordings,
                       std::vector<std::string>& problems) {
  Timeline timeline;
  bool first = true;
  for (const std::filesystem::path& path : recordings) {
    std::string name = path.filename().string();
    mcap::McapReader reader;
    mcap::Status status = reader.open(path.string());
    if (!status.ok()) {
      problems.push_back(name + ": cannot read the messages (" + status.message + ")");
      continue;
    }
    mcap::LinearMessageView messages = reader.readMessages();
    for (const mcap::MessageView& view : messages) {
      std::uint64_t time = view.message.logTime;
      timeline.times[view.channel->topic].push_back(time);
      if (first || time < timeline.start_ns) timeline.start_ns = time;
      if (first || time > timeline.end_ns) timeline.end_ns = time;
      first = false;
    }
    if (!messages.status().ok()) {
      problems.push_back(name + ": cannot read the messages (" + messages.status().message + ")");
    }
    reader.close();
  }
  return timeline;
}

TopicReport measure(const std::string& topic, std::vector<std::uint64_t> times,
                    std::uint64_t start_ns, std::uint64_t end_ns) {
  TopicReport report;
  report.topic = topic;
  report.messages = times.size();
  if (times.empty()) {  // silent for the whole recording
    report.max_gap_s = static_cast<double>(end_ns - start_ns) / kNsPerSecond;
    return report;
  }
  std::sort(times.begin(), times.end());  // messages from several files can come out of order

  double duration_s = static_cast<double>(end_ns - start_ns) / kNsPerSecond;
  if (duration_s > 0) {
    report.rate_hz = static_cast<double>(times.size() - 1) / duration_s;
  }

  // The longest gap: before the first message, between two messages, or after the last one.
  std::uint64_t max_gap = times.front() - start_ns;
  std::uint64_t gap_from = start_ns;
  for (std::size_t i = 1; i < times.size(); ++i) {
    if (times[i] - times[i - 1] > max_gap) {
      max_gap = times[i] - times[i - 1];
      gap_from = times[i - 1];
    }
  }
  if (end_ns - times.back() > max_gap) {
    max_gap = end_ns - times.back();
    gap_from = times.back();
  }
  report.max_gap_s = static_cast<double>(max_gap) / kNsPerSecond;
  report.max_gap_from_s = static_cast<double>(gap_from - start_ns) / kNsPerSecond;
  return report;
}

Status judge(const TopicReport& topic, const TopicRule& rule, std::vector<std::string>& problems) {
  if (topic.messages == 0) {
    if (!rule.required) return Status::Skip;
    problems.push_back(rule.topic + ": no messages (required topic)");
    return Status::Fail;
  }
  Status status = Status::Pass;
  if (rule.min_rate_hz && topic.rate_hz < *rule.min_rate_hz) {
    problems.push_back(rule.topic + ": rate " + fixed(topic.rate_hz, 2) + " Hz below minimum " +
                       plain(*rule.min_rate_hz) + " Hz");
    status = Status::Fail;
  }
  if (rule.max_gap_s && topic.max_gap_s > *rule.max_gap_s) {
    problems.push_back(rule.topic + ": silent for " + fixed(topic.max_gap_s, 3) +
                       " s starting at t=" + fixed(topic.max_gap_from_s, 3) + " s (limit " +
                       plain(*rule.max_gap_s) + " s)");
    status = Status::Fail;
  }
  return status;
}

CheckResult check_sensors(const std::vector<std::filesystem::path>& recordings,
                          const std::vector<TopicRule>& rules, std::vector<TopicReport>& topics) {
  CheckResult result;
  topics.clear();
  Timeline timeline = read_timeline(recordings, result.problems);
  if (!result.problems.empty()) {
    result.status = Status::Fail;
    result.summary = "could not read the messages";
    return result;
  }
  if (timeline.times.empty()) {
    result.fail("no messages in the recording");
    result.summary = "nothing to measure";
    return result;
  }

  for (const auto& [topic, times] : timeline.times) {
    topics.push_back(measure(topic, times, timeline.start_ns, timeline.end_ns));
  }
  for (const TopicRule& rule : rules) {
    auto row = std::find_if(topics.begin(), topics.end(),
                            [&rule](const TopicReport& t) { return t.topic == rule.topic; });
    if (row == topics.end()) {  // expected but never seen: still show it in the table
      row = topics.insert(topics.end(), measure(rule.topic, {}, timeline.start_ns, timeline.end_ns));
    }
    row->rule = rule;
    row->status = judge(*row, rule, result.problems);
  }
  std::sort(topics.begin(), topics.end(),
            [](const TopicReport& a, const TopicReport& b) { return a.topic < b.topic; });

  std::string count = std::to_string(topics.size()) + " topic(s)";
  if (!result.problems.empty()) {
    result.status = Status::Fail;
    result.summary = std::to_string(result.problems.size()) + " problem(s) in " + count;
  } else {
    result.status = Status::Pass;
    result.summary = count + (rules.empty() ? ", no rules given" : ", all rules met");
  }
  return result;
}

}  // namespace rosbagcheck
