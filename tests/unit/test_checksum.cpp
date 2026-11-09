// FR-1: SHA-256 and the SHA256SUMS format.
#include <gtest/gtest.h>

#include <sstream>

#include "checksum.hpp"

using rosbagcheck::parse_sums;

std::string sha256_of(const std::string& text) {
  std::istringstream in(text);
  return rosbagcheck::sha256_hex(in);
}

// Known answers published by NIST: if these pass, the hashing is wired up correctly.
TEST(Sha256, KnownAnswerEmptyInput) {
  EXPECT_EQ(sha256_of(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(Sha256, KnownAnswerAbc) {
  EXPECT_EQ(sha256_of("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

// One million bytes: larger than one read block, so the reading loop runs many times.
TEST(Sha256, KnownAnswerOneMillionA) {
  EXPECT_EQ(sha256_of(std::string(1000000, 'a')),
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

const std::string kHash(64, 'a');

TEST(ParseSums, ReadsTextAndBinaryMode) {
  auto sums = parse_sums(kHash + "  run_000.mcap\n" + kHash + " *run_001.mcap\n");
  ASSERT_TRUE(sums.problems.empty());
  ASSERT_EQ(sums.entries.size(), 2u);
  EXPECT_EQ(sums.entries[0].hash, kHash);
  EXPECT_EQ(sums.entries[0].name, "run_000.mcap");
  EXPECT_EQ(sums.entries[1].name, "run_001.mcap");
}

// `sha256sum ./*.mcap` writes "./run_000.mcap": the same file as "run_000.mcap".
TEST(ParseSums, DotSlashNameIsTheSameFile) {
  auto sums = parse_sums(kHash + "  ./run_000.mcap\n");
  ASSERT_EQ(sums.entries.size(), 1u);
  EXPECT_EQ(sums.entries[0].name, "run_000.mcap");
}

TEST(ParseSums, KeepsSpacesInFileNames) {
  auto sums = parse_sums(kHash + "  my run.mcap\n");
  ASSERT_EQ(sums.entries.size(), 1u);
  EXPECT_EQ(sums.entries[0].name, "my run.mcap");
}

TEST(ParseSums, UppercaseHashBecomesLowercase) {
  auto sums = parse_sums(std::string(64, 'A') + "  a.mcap\n");
  ASSERT_EQ(sums.entries.size(), 1u);
  EXPECT_EQ(sums.entries[0].hash, kHash);
}

TEST(ParseSums, IgnoresWindowsLineEndingsAndBlankLines) {
  auto sums = parse_sums(kHash + "  a.mcap\r\n\r\n\n");
  EXPECT_TRUE(sums.problems.empty());
  ASSERT_EQ(sums.entries.size(), 1u);
  EXPECT_EQ(sums.entries[0].name, "a.mcap");
}

// Every broken line becomes one problem that names its line number.
struct BadLine {
  std::string name;  // shown in the test name
  std::string line;
};

// Print only the name when a test fails, not the raw bytes of the struct.
void PrintTo(const BadLine& bad, std::ostream* out) { *out << bad.name; }

class ParseSumsBadLine : public ::testing::TestWithParam<BadLine> {};

TEST_P(ParseSumsBadLine, IsReportedWithItsLineNumber) {
  auto sums = parse_sums(kHash + "  good.mcap\n" + GetParam().line + "\n");
  EXPECT_EQ(sums.entries.size(), 1u);
  ASSERT_EQ(sums.problems.size(), 1u);
  EXPECT_NE(sums.problems[0].find("line 2"), std::string::npos) << sums.problems[0];
}

INSTANTIATE_TEST_SUITE_P(Malformed, ParseSumsBadLine,
                         ::testing::Values(BadLine{"ShortHash", std::string(63, 'a') + "  a.mcap"},
                                           BadLine{"NotHex", std::string(63, 'a') + "g  a.mcap"},
                                           BadLine{"TabSeparator", kHash + "\ta.mcap"},
                                           BadLine{"NoFileName", kHash + "  "},
                                           BadLine{"ParentFolder", kHash + "  ../a.mcap"},
                                           BadLine{"HiddenParentFolder", kHash + "  sub/../../a.mcap"},
                                           BadLine{"AbsolutePath", kHash + "  /etc/passwd"}),
                         [](const auto& info) { return info.param.name; });
