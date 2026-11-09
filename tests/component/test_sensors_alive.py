"""Check 3, sensors alive: did every sensor keep sending? (equivalence classes and boundary values)"""
import json

import pytest

from recordings import EXAMPLE_RULES as RULES
from recordings import healthy_topics, make_recording, periodic


def check(run, folder, *options):
    """Runs rosbagcheck --json on one folder: (exit code, sensors_alive result, topic rows by name)."""
    result = run("--json", *options, str(folder))
    recording = json.loads(result.stdout)["recordings"][0]
    rows = {row["topic"]: row for row in recording["topics"]}
    return result.returncode, recording["checks"]["sensors_alive"], rows


@pytest.mark.req("FR-3", "FR-4")
def test_healthy_recording_meets_every_rule(run, tmp_path):
    code, sensors, rows = check(run, make_recording(tmp_path / "run"), "--expect", str(RULES))
    assert code == 0, sensors
    assert sensors["summary"] == "4 topic(s), all rules met"
    assert rows["/camera/image_raw"]["messages"] == 201
    assert rows["/camera/image_raw"]["rate_hz"] == pytest.approx(20.0)
    assert rows["/odom"]["max_gap_s"] == pytest.approx(0.02)
    assert rows["/diagnostics"]["status"] == "SKIP"  # optional and not in the recording


@pytest.mark.req("FR-3", "NFR-5")
def test_camera_silent_for_four_seconds(run, tmp_path):
    topics = healthy_topics()
    topics["/camera/image_raw"] = periodic(20, silent=(3.0, 7.0))
    code, sensors, _ = check(run, make_recording(tmp_path / "run", topics), "--expect", str(RULES))
    assert code == 1
    # Both rules fire: 4 s of silence also pulls the 10 s average down to 12.1 Hz.
    assert sensors["problems"] == [
        "/camera/image_raw: rate 12.10 Hz below minimum 15 Hz",
        "/camera/image_raw: silent for 4.000 s starting at t=3.000 s (limit 0.5 s)"]


@pytest.mark.req("FR-3")
def test_short_dropout_is_caught_although_the_rate_is_fine(run, tmp_path):
    """Why max_gap_s exists: a 1 s dropout hardly changes the average rate."""
    topics = healthy_topics()
    topics["/camera/image_raw"] = periodic(20, silent=(5.0, 6.0))
    code, sensors, rows = check(run, make_recording(tmp_path / "run", topics), "--expect", str(RULES))
    assert code == 1
    assert rows["/camera/image_raw"]["rate_hz"] > 15  # the rate alone would say "fine"
    assert sensors["problems"] == [
        "/camera/image_raw: silent for 1.000 s starting at t=5.000 s (limit 0.5 s)"]


@pytest.mark.req("FR-3")
def test_sensor_that_dies_early_fails_rate_and_gap(run, tmp_path):
    """The rate counts the whole recording, so a sensor that stops at 6 s of 10 s gets 6 Hz."""
    topics = healthy_topics()
    topics["/scan"] = periodic(10, duration_s=6)
    code, sensors, rows = check(run, make_recording(tmp_path / "run", topics), "--expect", str(RULES))
    assert code == 1
    assert rows["/scan"]["rate_hz"] == pytest.approx(6.0)
    assert rows["/scan"]["max_gap_s"] == pytest.approx(4.0)
    assert sensors["problems"] == [
        "/scan: rate 6.00 Hz below minimum 8 Hz",
        "/scan: silent for 4.000 s starting at t=6.000 s (limit 0.5 s)"]


@pytest.mark.req("FR-3", "FR-4")
def test_required_topic_missing_fails(run, tmp_path):
    topics = healthy_topics()
    del topics["/scan"]
    code, sensors, rows = check(run, make_recording(tmp_path / "run", topics), "--expect", str(RULES))
    assert code == 1
    assert sensors["problems"] == ["/scan: no messages (required topic)"]
    assert rows["/scan"]["messages"] == 0  # still listed in the table


@pytest.mark.req("FR-3")
def test_topic_without_rule_is_listed_but_not_judged(run, tmp_path):
    topics = healthy_topics()
    topics["/imu"] = periodic(1)  # slow, but nobody set a rule for it
    code, _, rows = check(run, make_recording(tmp_path / "run", topics), "--expect", str(RULES))
    assert code == 0
    assert rows["/imu"]["rule"] is None
    assert rows["/imu"]["status"] == "SKIP"


GAP_RULE = "topics:\n  - topic: /scan\n    max_gap_s: {limit}\n"


@pytest.mark.req("FR-3", "FR-4")
@pytest.mark.parametrize("limit, code", [(0.5, 0), (0.501, 0), (0.499, 1)],
                         ids=["at limit", "above limit", "below limit"])
def test_gap_limit_is_inclusive(run, tmp_path, limit, code):
    """The longest gap is exactly 0.5 s."""
    folder = make_recording(tmp_path / "run", {"/scan": periodic(10, silent=(2.0, 2.5))})
    rules = tmp_path / "rules.yaml"
    rules.write_text(GAP_RULE.format(limit=limit))
    assert check(run, folder, "--expect", str(rules))[0] == code


@pytest.mark.req("FR-3", "FR-4")
@pytest.mark.parametrize("rate, code", [(10, 0), (9.9, 1)], ids=["at minimum", "just below"])
def test_rate_limit_is_inclusive(run, tmp_path, rate, code):
    folder = make_recording(tmp_path / "run", {"/scan": periodic(rate)})
    rules = tmp_path / "rules.yaml"
    rules.write_text("topics:\n  - topic: /scan\n    min_rate_hz: 10\n")
    assert check(run, folder, "--expect", str(rules))[0] == code


@pytest.mark.req("FR-3")
def test_gap_across_two_files_counts(run, tmp_path):
    """A recording split into files: the silence across the files is still one gap."""
    topics = healthy_topics()
    topics["/camera/image_raw"] = periodic(20, silent=(4.0, 6.0))
    folder = make_recording(tmp_path / "run", topics, files=2)
    code, _, rows = check(run, folder, "--expect", str(RULES))
    assert code == 1
    assert rows["/camera/image_raw"]["max_gap_s"] == pytest.approx(2.0)


@pytest.mark.req("FR-3")
def test_without_rules_only_measures(run, tmp_path):
    code, sensors, rows = check(run, make_recording(tmp_path / "run"))
    assert code == 0
    assert sensors["summary"] == "3 topic(s), no rules given"
    assert all(row["status"] == "SKIP" for row in rows.values())


@pytest.mark.req("FR-3")
def test_recording_without_messages_fails(run, tmp_path):
    code, sensors, _ = check(run, make_recording(tmp_path / "run", topics={}))
    assert code == 1
    assert sensors["problems"] == ["no messages in the recording"]


@pytest.mark.req("FR-2", "FR-3")
def test_skipped_when_not_closed_properly(run, tmp_path):
    code, sensors, rows = check(run, make_recording(tmp_path / "run", closed=False))
    assert code == 1
    assert sensors["status"] == "SKIP"
    assert sensors["summary"] == "skipped: not every file closed properly"
    assert rows == {}


@pytest.mark.req("FR-3", "FR-5")
def test_text_report_shows_the_topic_table(run, tmp_path):
    result = run("--expect", str(RULES), str(make_recording(tmp_path / "run")))
    lines = result.stdout.splitlines()
    assert any(line.split()[:5] == ["topic", "messages", "rate", "Hz", "max"] for line in lines)
    camera = next(line for line in lines if line.strip().startswith("/camera/image_raw"))
    assert camera.split() == ["/camera/image_raw", "201", "20.00", "0.050",
                              ">=15", "Hz", "gap<=0.5", "s", "required", "PASS"]
