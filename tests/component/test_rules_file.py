"""The rules file: bad input is an input error (exit 2) before anything is checked, never a verdict."""
import pytest

from recordings import EXAMPLE_RULES as RULES
from recordings import make_recording


@pytest.mark.req("FR-4", "FR-7", "NFR-5")
@pytest.mark.parametrize("text, message", [
    ("topics: [", "not valid YAML"),
    ("rules: []", "unknown top-level key 'rules'"),
    ("topics:\n  - topic: /scan\n    min_rate: 10\n", "unknown key 'min_rate'"),
    ("topics:\n  - topic: /scan\n    max_gap_s: -1\n", "must be a number greater than 0"),
    ("topics:\n  - topic: /scan\n  - topic: /scan\n", "listed twice"),
], ids=["broken YAML", "wrong top-level key", "typo in key", "negative limit", "duplicate topic"])
def test_bad_rules_file_exits_2_with_message(run, tmp_path, text, message):
    rules = tmp_path / "rules.yaml"
    rules.write_text(text)
    result = run("--expect", str(rules), str(make_recording(tmp_path / "run")))
    assert result.returncode == 2
    assert result.stdout == ""  # nothing half-checked on stdout
    assert message in result.stderr
    assert str(rules) in result.stderr  # says which file is wrong


@pytest.mark.req("FR-4", "FR-7")
def test_rules_file_that_does_not_exist_exits_2(run, tmp_path):
    result = run("--expect", str(tmp_path / "nope.yaml"), str(make_recording(tmp_path / "run")))
    assert result.returncode == 2
    assert "cannot read the file" in result.stderr


@pytest.mark.req("FR-4")
def test_example_rules_file_is_valid(run, tmp_path):
    assert run("--expect", str(RULES), str(make_recording(tmp_path / "run"))).returncode == 0
