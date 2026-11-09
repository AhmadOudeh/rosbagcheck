#include "report.hpp"

#include <iomanip>
#include <sstream>

namespace rosbagcheck {

void CheckResult::fail(const std::string& problem) {
  status = Status::Fail;
  problems.push_back(problem);
}

bool RecordingReport::healthy() const {
  return copy.status == Status::Pass && closed.status == Status::Pass &&
         sensors.status == Status::Pass;
}

namespace {

std::string name(Status status) {
  switch (status) {
    case Status::Pass: return "PASS";
    case Status::Fail: return "FAIL";
    case Status::Skip: return "SKIP";
  }
  return "?";
}

// One line per check, then one indented line per problem:
//   FAIL  complete copy     2 of 3 files ok
//         - run_000.mcap: checksum differs
std::string check_lines(const std::string& label, const CheckResult& check) {
  std::string padded = label + std::string(18 - label.size(), ' ');
  std::string text = "  " + name(check.status) + "  " + padded + check.summary + "\n";
  for (const std::string& problem : check.problems) {
    text += "        - " + problem + "\n";
  }
  return text;
}

nlohmann::json check_json(const CheckResult& check) {
  return {{"status", name(check.status)}, {"summary", check.summary}, {"problems", check.problems}};
}

// The rule of one topic in short form, e.g. ">=15 Hz gap<=0.5 s required", or "-" without a rule.
std::string rule_text(const std::optional<TopicRule>& rule) {
  if (!rule) return "-";
  std::ostringstream text;
  if (rule->min_rate_hz) text << ">=" << *rule->min_rate_hz << " Hz ";
  if (rule->max_gap_s) text << "gap<=" << *rule->max_gap_s << " s ";
  text << (rule->required ? "required" : "optional");
  return text.str();
}

// The topic table under the checks. Long names push the other columns right, never into them.
//   topic                   messages    rate Hz   max gap s  rule                          result
//   /scan                        101      10.00       0.100  >=8 Hz gap<=0.5 s required    PASS
std::string topic_table(const std::vector<TopicReport>& topics) {
  std::ostringstream text;
  text << std::left << "\n  " << std::setw(22) << "topic" << std::right << std::setw(10)
       << "messages" << std::setw(11) << "rate Hz" << std::setw(12) << "max gap s" << "  "
       << std::left << std::setw(28) << "rule" << "  result\n";
  for (const TopicReport& topic : topics) {
    text << std::left << "  " << std::setw(22) << topic.topic << std::right << std::setw(10)
         << topic.messages << std::fixed << std::setprecision(2) << std::setw(11) << topic.rate_hz
         << std::setprecision(3) << std::setw(12) << topic.max_gap_s << "  " << std::left
         << std::setw(28) << rule_text(topic.rule) << "  " << name(topic.status) << "\n";
  }
  return text.str() + "\n";
}

nlohmann::json topic_json(const TopicReport& topic) {
  nlohmann::json rule = nullptr;
  if (topic.rule) {
    rule = {{"min_rate_hz", nullptr}, {"max_gap_s", nullptr}, {"required", topic.rule->required}};
    if (topic.rule->min_rate_hz) rule["min_rate_hz"] = *topic.rule->min_rate_hz;
    if (topic.rule->max_gap_s) rule["max_gap_s"] = *topic.rule->max_gap_s;
  }
  return {{"topic", topic.topic},
          {"messages", topic.messages},
          {"rate_hz", topic.rate_hz},
          {"max_gap_s", topic.max_gap_s},
          {"max_gap_from_s", topic.max_gap_from_s},
          {"rule", rule},
          {"status", name(topic.status)}};
}

}  // namespace

std::string render_text(const RecordingReport& report) {
  return "recording " + report.path + "\n" +
         check_lines("complete copy", report.copy) +
         check_lines("properly closed", report.closed) +
         check_lines("sensors alive", report.sensors) +
         (report.topics.empty() ? "" : topic_table(report.topics)) +
         "verdict " + (report.healthy() ? "HEALTHY" : "UNHEALTHY") + "\n";
}

nlohmann::json to_json(const RecordingReport& report) {
  nlohmann::json topics = nlohmann::json::array();
  for (const TopicReport& topic : report.topics) {
    topics.push_back(topic_json(topic));
  }
  return {{"path", report.path},
          {"healthy", report.healthy()},
          {"checks",
           {{"complete_copy", check_json(report.copy)},
            {"properly_closed", check_json(report.closed)},
            {"sensors_alive", check_json(report.sensors)}}},
          {"topics", topics}};
}

}  // namespace rosbagcheck
