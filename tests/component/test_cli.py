"""The command line itself: options, help, exit codes, JSON shape."""
import json

import pytest


@pytest.mark.req("FR-7")
def test_version_exits_0(run):
    result = run("--version")
    assert result.returncode == 0
    assert result.stdout.strip() == "rosbagcheck 0.1.0"


@pytest.mark.req("FR-7", "NFR-5")
def test_help_explains_checks_and_exit_codes(run):
    result = run("--help")
    assert result.returncode == 0
    for words in ["complete copy", "properly closed", "sensors alive", "0 healthy", "1 unhealthy", "2 usage"]:
        assert words in result.stdout


@pytest.mark.req("FR-7")
@pytest.mark.parametrize("args", [[], ["--no-such-option", "."]], ids=["no folder", "unknown option"])
def test_usage_error_exits_2(run, args):
    result = run(*args)
    assert result.returncode == 2
    assert result.stderr != ""


@pytest.mark.req("FR-7", "NFR-5")
def test_missing_folder_exits_2_and_names_it(run, tmp_path):
    missing = tmp_path / "run_042"
    result = run(str(missing))
    assert result.returncode == 2
    assert str(missing) in result.stderr
    assert result.stdout == ""


@pytest.mark.req("FR-7")
def test_one_missing_folder_stops_before_any_report(run, tmp_path):
    result = run(str(tmp_path), str(tmp_path / "typo"))
    assert result.returncode == 2
    assert result.stdout == ""


@pytest.mark.req("FR-6", "FR-8")
def test_json_has_one_entry_per_folder_in_order(run, tmp_path):
    first, second = tmp_path / "run_a", tmp_path / "run_b"
    first.mkdir()
    second.mkdir()
    report = json.loads(run("--json", str(first), str(second)).stdout)
    assert report["rosbagcheck_version"] == "0.1.0"
    assert [r["path"] for r in report["recordings"]] == [str(first), str(second)]
    assert set(report["recordings"][0]["checks"]) == {"complete_copy", "properly_closed", "sensors_alive"}


@pytest.mark.req("FR-5", "FR-8")
def test_text_report_per_folder_in_order(run, tmp_path):
    first, second = tmp_path / "run_a", tmp_path / "run_b"
    first.mkdir()
    second.mkdir()
    text = run(str(first), str(second)).stdout
    assert text.index(f"recording {first}") < text.index(f"recording {second}")
    assert text.count("verdict") == 2
