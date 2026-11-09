// FR-4: the rules file. A typo must be an error, never a silently ignored rule.
#include <gtest/gtest.h>

#include "rules.hpp"

using rosbagcheck::parse_rules;
using rosbagcheck::RulesError;
using rosbagcheck::TopicRule;

TEST(ParseRules, ReadsEveryKey) {
  std::vector<TopicRule> rules = parse_rules(
      "topics:\n"
      "  - topic: /camera/image_raw\n"
      "    min_rate_hz: 15\n"
      "    max_gap_s: 0.5\n"
      "  - topic: /diagnostics\n"
      "    required: false\n");
  ASSERT_EQ(rules.size(), 2u);
  EXPECT_EQ(rules[0].topic, "/camera/image_raw");
  EXPECT_DOUBLE_EQ(*rules[0].min_rate_hz, 15.0);
  EXPECT_DOUBLE_EQ(*rules[0].max_gap_s, 0.5);
  EXPECT_TRUE(rules[0].required);
  EXPECT_FALSE(rules[1].min_rate_hz.has_value());
  EXPECT_FALSE(rules[1].max_gap_s.has_value());
  EXPECT_FALSE(rules[1].required);
}

TEST(ParseRules, EmptyListIsAllowed) { EXPECT_TRUE(parse_rules("topics: []\n").empty()); }

// Each bad file must be rejected with a message that says what is wrong.
struct BadRules {
  const char* name;
  const char* yaml;
  const char* message;
};

void PrintTo(const BadRules& bad, std::ostream* out) { *out << bad.name; }

class ParseRulesBad : public testing::TestWithParam<BadRules> {};

TEST_P(ParseRulesBad, IsRejectedWithAClearMessage) {
  try {
    parse_rules(GetParam().yaml);
    FAIL() << "no error for: " << GetParam().yaml;
  } catch (const RulesError& error) {
    EXPECT_NE(std::string(error.what()).find(GetParam().message), std::string::npos)
        << error.what();
  }
}

INSTANTIATE_TEST_SUITE_P(
    Bad, ParseRulesBad,
    testing::Values(
        BadRules{"BrokenYaml", "topics: [", "not valid YAML"},
        BadRules{"EmptyFile", "", "expected a top-level 'topics:' list"},
        BadRules{"NoTopicsList", "topics: /scan\n", "expected a top-level 'topics:' list"},
        BadRules{"UnknownTopLevelKey", "topics: []\nrule: 1\n", "unknown top-level key 'rule'"},
        BadRules{"TypoInKey", "topics:\n  - topic: /scan\n    min_rate: 10\n",
                 "topics[0]: unknown key 'min_rate'"},
        BadRules{"TopicMissing", "topics:\n  - min_rate_hz: 10\n", "'topic' is missing"},
        BadRules{"TopicListedTwice", "topics:\n  - topic: /scan\n  - topic: /scan\n",
                 "topics[1]: topic '/scan' is listed twice"},
        BadRules{"RateIsText", "topics:\n  - topic: /scan\n    min_rate_hz: fast\n",
                 "topics[0].min_rate_hz must be a number"},
        BadRules{"RateZero", "topics:\n  - topic: /scan\n    min_rate_hz: 0\n",
                 "must be a number greater than 0"},
        BadRules{"GapNegative", "topics:\n  - topic: /scan\n    max_gap_s: -1\n",
                 "must be a number greater than 0"},
        BadRules{"GapInfinite", "topics:\n  - topic: /scan\n    max_gap_s: .inf\n",
                 "must be a number greater than 0"},
        BadRules{"RequiredNotBool", "topics:\n  - topic: /scan\n    required: maybe\n",
                 "required must be true or false"},
        BadRules{"KeyIsAList", "topics:\n  - ? [a, b]\n    : 1\n", "every key must be plain text"}),
    [](const testing::TestParamInfo<BadRules>& info) { return info.param.name; });
