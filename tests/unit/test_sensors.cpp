// FR-3: rate and gap maths, and the limits (inclusive).
#include <gtest/gtest.h>

#include "sensors.hpp"

using rosbagcheck::FileSpan;
using rosbagcheck::find_overlaps;
using rosbagcheck::judge;
using rosbagcheck::measure;
using rosbagcheck::Status;
using rosbagcheck::TopicReport;
using rosbagcheck::TopicRule;

const std::uint64_t kSecond = 1'000'000'000;

// `count` messages at `rate_hz`, the first one at `first_ns`.
std::vector<std::uint64_t> periodic(std::uint64_t rate_hz, int count, std::uint64_t first_ns = 0) {
  std::vector<std::uint64_t> times;
  for (int i = 0; i < count; ++i) {
    times.push_back(first_ns + i * (kSecond / rate_hz));
  }
  return times;
}

TEST(Measure, SteadyTenHertz) {
  TopicReport topic = measure("/scan", periodic(10, 11), 0, kSecond);
  EXPECT_EQ(topic.messages, 11u);
  EXPECT_DOUBLE_EQ(topic.rate_hz, 10.0);
  EXPECT_DOUBLE_EQ(topic.max_gap_s, 0.1);
}

TEST(Measure, FindsTheLongestGapAndWhereItStarts) {
  TopicReport topic = measure("/scan", {0, kSecond, 5 * kSecond, 6 * kSecond}, 0, 6 * kSecond);
  EXPECT_DOUBLE_EQ(topic.max_gap_s, 4.0);
  EXPECT_DOUBLE_EQ(topic.max_gap_from_s, 1.0);
}

TEST(Measure, MessagesOutOfOrderAreSorted) {  // several files can deliver them out of order
  TopicReport topic = measure("/scan", {2 * kSecond, 0, kSecond}, 0, 2 * kSecond);
  EXPECT_DOUBLE_EQ(topic.max_gap_s, 1.0);
}

// The sensor stops after 1 s of a 10 s recording: the silence at the end is the longest gap,
// and the rate counts the whole recording, so it drops to 1 Hz.
TEST(Measure, SensorThatDiesEarlyHasAGapAtTheEndAndALowRate) {
  TopicReport topic = measure("/scan", periodic(10, 11), 0, 10 * kSecond);
  EXPECT_DOUBLE_EQ(topic.max_gap_s, 9.0);
  EXPECT_DOUBLE_EQ(topic.max_gap_from_s, 1.0);
  EXPECT_DOUBLE_EQ(topic.rate_hz, 1.0);
}

TEST(Measure, SensorThatStartsLateHasAGapAtTheStart) {
  TopicReport topic = measure("/scan", periodic(10, 11, 3 * kSecond), 0, 4 * kSecond);
  EXPECT_DOUBLE_EQ(topic.max_gap_s, 3.0);
  EXPECT_DOUBLE_EQ(topic.max_gap_from_s, 0.0);
}

TEST(Measure, SingleMessageHasNoRate) {
  TopicReport topic = measure("/scan", {kSecond}, kSecond, kSecond);
  EXPECT_EQ(topic.messages, 1u);
  EXPECT_DOUBLE_EQ(topic.rate_hz, 0.0);
}

TEST(Measure, NoMessagesMeansSilentTheWholeRecording) {
  TopicReport topic = measure("/scan", {}, 0, 5 * kSecond);
  EXPECT_EQ(topic.messages, 0u);
  EXPECT_DOUBLE_EQ(topic.max_gap_s, 5.0);
}

// Boundary values for judge: the limits are inclusive.
TopicReport measured(double rate_hz, double max_gap_s) {
  TopicReport topic;
  topic.topic = "/scan";
  topic.messages = 100;
  topic.rate_hz = rate_hz;
  topic.max_gap_s = max_gap_s;
  return topic;
}

TopicRule rule(std::optional<double> min_rate_hz, std::optional<double> max_gap_s,
               bool required = true) {
  return TopicRule{"/scan", min_rate_hz, max_gap_s, required};
}

TEST(Judge, RateExactlyAtMinimumPasses) {
  std::vector<std::string> problems;
  EXPECT_EQ(judge(measured(10.0, 0.1), rule(10.0, {}), problems), Status::Pass);
  EXPECT_TRUE(problems.empty());
}

TEST(Judge, RateJustBelowMinimumFails) {
  std::vector<std::string> problems;
  EXPECT_EQ(judge(measured(9.99, 0.1), rule(10.0, {}), problems), Status::Fail);
  ASSERT_EQ(problems.size(), 1u);
  EXPECT_EQ(problems[0], "/scan: rate 9.99 Hz below minimum 10 Hz");
}

TEST(Judge, GapExactlyAtLimitPasses) {
  std::vector<std::string> problems;
  EXPECT_EQ(judge(measured(10.0, 0.5), rule({}, 0.5), problems), Status::Pass);
}

TEST(Judge, GapOneMillisecondOverLimitFails) {
  std::vector<std::string> problems;
  EXPECT_EQ(judge(measured(10.0, 0.501), rule({}, 0.5), problems), Status::Fail);
  ASSERT_EQ(problems.size(), 1u);
  EXPECT_EQ(problems[0], "/scan: silent for 0.501 s starting at t=0.000 s (limit 0.5 s)");
}

TEST(Judge, BothLimitsBrokenGivesTwoProblems) {
  std::vector<std::string> problems;
  EXPECT_EQ(judge(measured(1.0, 4.0), rule(10.0, 0.5), problems), Status::Fail);
  EXPECT_EQ(problems.size(), 2u);
}

TEST(Judge, MissingRequiredTopicFails) {
  std::vector<std::string> problems;
  TopicReport none = measured(0, 0);
  none.messages = 0;
  EXPECT_EQ(judge(none, rule({}, {}), problems), Status::Fail);
  ASSERT_EQ(problems.size(), 1u);
  EXPECT_EQ(problems[0], "/scan: no messages (required topic)");
}

TEST(Judge, MissingOptionalTopicIsSkipped) {
  std::vector<std::string> problems;
  TopicReport none = measured(0, 0);
  none.messages = 0;
  EXPECT_EQ(judge(none, rule(10.0, {}, false), problems), Status::Skip);
  EXPECT_TRUE(problems.empty());
}

TEST(Judge, OptionalTopicThatIsPresentIsStillJudged) {
  std::vector<std::string> problems;
  EXPECT_EQ(judge(measured(1.0, 0.1), rule(10.0, {}, false), problems), Status::Fail);
}

// Overlapping files: the same data in two files would be counted twice.
TEST(FindOverlaps, FilesOneAfterAnotherAreFine) {
  EXPECT_TRUE(find_overlaps({{"a", 0, kSecond}, {"b", 2 * kSecond, 3 * kSecond}}).empty());
}

TEST(FindOverlaps, TouchingFilesAreFine) {  // boundary: last of one == first of the next
  EXPECT_TRUE(find_overlaps({{"a", 0, kSecond}, {"b", kSecond, 2 * kSecond}}).empty());
}

TEST(FindOverlaps, IdenticalCopyIsFound) {
  std::vector<std::string> problems = find_overlaps({{"a", 0, kSecond}, {"b", 0, kSecond}});
  ASSERT_EQ(problems.size(), 1u);
  EXPECT_EQ(problems[0].rfind("a and b overlap in time", 0), 0u);
}

TEST(FindOverlaps, OneNanosecondOverlapIsFoundInAnyFileOrder) {
  EXPECT_EQ(find_overlaps({{"b", kSecond - 1, 2 * kSecond}, {"a", 0, kSecond}}).size(), 1u);
}
