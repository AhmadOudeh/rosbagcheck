#include "report.hpp"

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

}  // namespace

std::string render_text(const RecordingReport& report) {
  return "recording " + report.path + "\n" +
         check_lines("complete copy", report.copy) +
         check_lines("properly closed", report.closed) +
         check_lines("sensors alive", report.sensors) +
         "verdict " + (report.healthy() ? "HEALTHY" : "UNHEALTHY") + "\n";
}

nlohmann::json to_json(const RecordingReport& report) {
  return {{"path", report.path},
          {"healthy", report.healthy()},
          {"checks",
           {{"complete_copy", check_json(report.copy)},
            {"properly_closed", check_json(report.closed)},
            {"sensors_alive", check_json(report.sensors)}}}};
}

}  // namespace rosbagcheck
