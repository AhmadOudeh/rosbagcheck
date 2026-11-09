#include "rules.hpp"

#include <yaml-cpp/yaml.h>

#include <cmath>
#include <fstream>
#include <set>
#include <sstream>

namespace rosbagcheck {

namespace {

// The text of a map key. A key that is not plain text (e.g. a list) is an error.
std::string key_name(const YAML::Node& key, const std::string& where) {
  if (!key.IsScalar()) throw RulesError(where + ": every key must be plain text");
  return key.Scalar();
}

// A limit must be a real number greater than 0 (not text, not .inf, not .nan).
double limit(const YAML::Node& node, const std::string& where) {
  double value = 0.0;
  if (!node.IsScalar() || !YAML::convert<double>::decode(node, value)) {
    throw RulesError(where + " must be a number");
  }
  if (!std::isfinite(value) || value <= 0.0) {
    throw RulesError(where + " must be a number greater than 0");
  }
  return value;
}

TopicRule parse_rule(const YAML::Node& item, const std::string& where) {
  if (!item.IsMap()) throw RulesError(where + " must be a map with a 'topic' key");
  TopicRule rule;
  bool has_topic = false;
  for (const auto& pair : item) {
    std::string key = key_name(pair.first, where);
    const YAML::Node& value = pair.second;
    if (key == "topic") {
      if (!value.IsScalar() || value.Scalar().empty()) {
        throw RulesError(where + ".topic must be a topic name");
      }
      rule.topic = value.Scalar();
      has_topic = true;
    } else if (key == "min_rate_hz") {
      rule.min_rate_hz = limit(value, where + ".min_rate_hz");
    } else if (key == "max_gap_s") {
      rule.max_gap_s = limit(value, where + ".max_gap_s");
    } else if (key == "required") {
      if (!value.IsScalar() || !YAML::convert<bool>::decode(value, rule.required)) {
        throw RulesError(where + ".required must be true or false");
      }
    } else {
      // Usually a typo (min_rate instead of min_rate_hz): ignoring it would turn the rule off.
      throw RulesError(where + ": unknown key '" + key + "'");
    }
  }
  if (!has_topic) throw RulesError(where + ": 'topic' is missing");
  return rule;
}

}  // namespace

std::vector<TopicRule> parse_rules(const std::string& yaml_text) {
  YAML::Node root;
  try {
    root = YAML::Load(yaml_text);
  } catch (const YAML::Exception& error) {
    throw RulesError("not valid YAML: " + error.msg + " (line " +
                     std::to_string(error.mark.line + 1) + ")");
  }
  if (!root.IsMap()) throw RulesError("expected a top-level 'topics:' list");
  for (const auto& pair : root) {
    std::string key = key_name(pair.first, "top level");
    if (key != "topics") throw RulesError("unknown top-level key '" + key + "'");
  }
  YAML::Node topics = root["topics"];
  if (!topics.IsSequence()) throw RulesError("expected a top-level 'topics:' list");

  std::vector<TopicRule> rules;
  std::set<std::string> seen;
  for (std::size_t i = 0; i < topics.size(); ++i) {
    std::string where = "topics[" + std::to_string(i) + "]";
    TopicRule rule = parse_rule(topics[i], where);
    if (!seen.insert(rule.topic).second) {
      throw RulesError(where + ": topic '" + rule.topic + "' is listed twice");
    }
    rules.push_back(rule);
  }
  return rules;
}

std::vector<TopicRule> load_rules(const std::string& path) {
  std::ifstream file(path);
  if (!file) throw RulesError("cannot read the file");
  std::stringstream text;
  text << file.rdbuf();
  return parse_rules(text.str());
}

}  // namespace rosbagcheck
