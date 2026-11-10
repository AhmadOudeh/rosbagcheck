"""examples/make_demo.py: the README quick start gives the results it promises."""
import subprocess
import sys

import pytest

from recordings import EXAMPLE_RULES

SCRIPT = EXAMPLE_RULES.parent / "make_demo.py"


@pytest.mark.req("FR-5", "FR-7")
@pytest.mark.parametrize("name, exit_code", [("run_ok", 0), ("run_cut_copy", 1), ("run_lidar_stops", 1)])
def test_demo_runs_give_the_documented_verdict(run, tmp_path, name, exit_code):
    subprocess.run([sys.executable, str(SCRIPT), str(tmp_path)], check=True, capture_output=True)
    done = run("--expect", str(EXAMPLE_RULES), str(tmp_path / name))
    assert done.returncode == exit_code
