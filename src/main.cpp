// rosbagcheck: checks ROS 2 field-test recording folders before anyone trusts them.
#include <CLI/CLI.hpp>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "checksum.hpp"
#include "closed.hpp"
#include "report.hpp"
#include "rules.hpp"
#include "sensors.hpp"

namespace {

const int kHealthy = 0;
const int kUnhealthy = 1;
const int kInputError = 2;

}  // namespace

int main(int argc, char** argv) {
  CLI::App app{
      "rosbagcheck: checks ROS 2 recording folders (MCAP) before anyone trusts them.\n"
      "  1 complete copy    every file matches SHA256SUMS\n"
      "  2 properly closed  every .mcap file has its summary and footer\n"
      "  3 sensors alive    message rate and longest gap per topic\n"
      "Exit codes: 0 healthy, 1 unhealthy, 2 usage or input error."};
  app.set_version_flag("--version", "rosbagcheck " ROSBAGCHECK_VERSION);

  std::vector<std::string> folders;
  std::string rules_path;
  bool json = false;
  app.add_option("folders", folders, "Recording folders to check")->required();
  app.add_option("--expect", rules_path, "Rules file (YAML): limits per topic for check 3");
  app.add_flag("--json", json, "Print a JSON report instead of text");

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError& error) {
    // app.exit prints the help, the version or the error message.
    return app.exit(error) == 0 ? kHealthy : kInputError;
  }

  // Check all input first, so a typo in the last folder does not waste a long run.
  std::vector<rosbagcheck::TopicRule> rules;
  if (!rules_path.empty()) {
    try {
      rules = rosbagcheck::load_rules(rules_path);
    } catch (const rosbagcheck::RulesError& error) {
      std::cerr << "rosbagcheck: error: " << rules_path << ": " << error.what() << "\n";
      return kInputError;
    }
  }
  for (const std::string& folder : folders) {
    if (!std::filesystem::is_directory(folder)) {
      std::cerr << "rosbagcheck: error: not a folder: " << folder << "\n";
      return kInputError;
    }
  }

  bool all_healthy = true;
  nlohmann::json recordings = nlohmann::json::array();
  for (const std::string& folder : folders) {
    rosbagcheck::RecordingReport report;
    report.path = folder;
    report.copy = rosbagcheck::check_copy(folder);
    report.closed = rosbagcheck::check_closed(folder);
    if (report.closed.status == rosbagcheck::Status::Pass) {
      report.sensors =
          rosbagcheck::check_sensors(rosbagcheck::list_recordings(folder), rules, report.topics);
    } else {
      // A file that was not closed may end anywhere: its numbers would mislead.
      report.sensors.summary = "skipped: not every file closed properly";
    }

    all_healthy = all_healthy && report.healthy();
    if (json) {
      recordings.push_back(rosbagcheck::to_json(report));
    } else {
      std::cout << rosbagcheck::render_text(report);
    }
  }

  if (json) {
    nlohmann::json out = {{"rosbagcheck_version", ROSBAGCHECK_VERSION},
                          {"healthy", all_healthy},
                          {"recordings", recordings}};
    std::cout << out.dump(2) << "\n";
  }
  return all_healthy ? kHealthy : kUnhealthy;
}
