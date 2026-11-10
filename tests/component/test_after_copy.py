"""examples/after_copy.sh: a shell script can decide "delete or keep" from the exit code alone."""
import os
import subprocess
from pathlib import Path

import pytest

from recordings import EXAMPLE_RULES, make_recording

SCRIPT = EXAMPLE_RULES.parent / "after_copy.sh"


@pytest.fixture
def after_copy(request):
    """after_copy(*args) runs the example script with the rosbagcheck under test."""
    binary = Path(request.config.getoption("--binary")).resolve()
    env = dict(os.environ, ROSBAGCHECK=str(binary))

    def _run(*args):
        return subprocess.run([str(SCRIPT), *map(str, args)], capture_output=True, text=True,
                              env=env, timeout=60)

    return _run


@pytest.mark.req("FR-7")
def test_healthy_runs_are_safe_to_delete(after_copy, tmp_path):
    runs = [make_recording(tmp_path / f"run_{i}") for i in range(2)]
    done = after_copy(EXAMPLE_RULES, *runs)
    assert done.returncode == 0
    assert done.stdout.count("safe to delete") == 2


@pytest.mark.req("FR-7")
def test_cut_off_copy_is_kept(after_copy, tmp_path):
    good = make_recording(tmp_path / "good")
    broken = make_recording(tmp_path / "broken")
    with open(broken / "run_000.mcap", "r+b") as f:
        f.truncate(f.seek(0, 2) - 100)  # the scp copy was cut off
    done = after_copy(EXAMPLE_RULES, good, broken)
    assert done.returncode == 1
    assert f"OK    {good}" in done.stdout
    assert f"KEEP  {broken}" in done.stdout


@pytest.mark.req("FR-7")
def test_bad_rules_file_stops_the_script(after_copy, tmp_path):
    rules = tmp_path / "rules.yaml"
    rules.write_text("topics: [")
    done = after_copy(rules, make_recording(tmp_path / "run"))
    assert done.returncode == 2
    assert "not valid YAML" in done.stderr
    assert "safe to delete" not in done.stdout
