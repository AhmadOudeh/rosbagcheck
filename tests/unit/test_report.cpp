#include <gtest/gtest.h>

#include "report.hpp"

using rosbagcheck::CheckResult;
using rosbagcheck::RecordingReport;
using rosbagcheck::Status;

RecordingReport all_passed() {
  RecordingReport report;
  report.copy.status = Status::Pass;
  report.closed.status = Status::Pass;
  report.sensors.status = Status::Pass;
  return report;
}

TEST(CheckResult, StartsAsSkippedNotPassed) {
  CheckResult check;
  EXPECT_EQ(check.status, Status::Skip);
}

TEST(CheckResult, FailSetsStatusAndKeepsEveryProblem) {
  CheckResult check;
  check.fail("a.mcap: missing");
  check.fail("b.mcap: checksum differs");
  EXPECT_EQ(check.status, Status::Fail);
  EXPECT_EQ(check.problems.size(), 2u);
}

TEST(RecordingReport, HealthyWhenAllChecksPass) {
  EXPECT_TRUE(all_passed().healthy());
}

TEST(RecordingReport, UnhealthyWhenOneCheckFails) {
  RecordingReport report = all_passed();
  report.closed.fail("run_000.mcap: no footer");
  EXPECT_FALSE(report.healthy());
}

// The worst case is a false HEALTHY: a check that did not run must never count as passed.
TEST(RecordingReport, UnhealthyWhenOneCheckWasSkipped) {
  RecordingReport report = all_passed();
  report.sensors.status = Status::Skip;
  EXPECT_FALSE(report.healthy());
}

TEST(RenderText, ShowsEveryProblemAndTheVerdict) {
  RecordingReport report = all_passed();
  report.path = "run_042";
  report.copy.fail("run_000.mcap: missing");
  std::string text = rosbagcheck::render_text(report);
  EXPECT_NE(text.find("recording run_042"), std::string::npos);
  EXPECT_NE(text.find("FAIL  complete copy"), std::string::npos);
  EXPECT_NE(text.find("- run_000.mcap: missing"), std::string::npos);
  EXPECT_NE(text.find("verdict UNHEALTHY"), std::string::npos);
}
