"""Shared helpers: every component test runs the real rosbagcheck program."""
import platform
import re
import subprocess
from pathlib import Path

import pytest


def pytest_addoption(parser):
    parser.addoption("--binary", default="build/rosbagcheck", help="path to the rosbagcheck program")


@pytest.fixture
def run(request):
    """run("folder", "--json") starts rosbagcheck and returns the finished process."""
    binary = request.config.getoption("--binary")

    def _run(*args):
        return subprocess.run([binary, *args], capture_output=True, text=True, timeout=60)

    return _run


# Requirement table: after the run, one row per requirement in docs/REQUIREMENTS.md with the result
# of its tests. Printed at the end and written to build/requirements.md (CI shows it on the run page).
ROOT = Path(__file__).resolve().parents[2]
REQUIREMENT_ROW = re.compile(r"^\| ((?:N?FR)-\d+) \| \*\*(.+?)\.?\*\*")
results = {}  # requirement ID -> list of test outcomes ("passed", "failed", "skipped")


def requirements():
    """(ID, short title) for every requirement, in the order of docs/REQUIREMENTS.md."""
    text = (ROOT / "docs" / "REQUIREMENTS.md").read_text()
    return [match.groups() for match in map(REQUIREMENT_ROW.match, text.splitlines()) if match]


@pytest.hookimpl(wrapper=True)
def pytest_runtest_makereport(item, call):
    report = yield
    marker = item.get_closest_marker("req")
    # One outcome per test: the "call" phase, or the setup phase if setup already failed or skipped.
    if marker and (report.when == "call" or report.outcome != "passed"):
        for requirement in marker.args:
            results.setdefault(requirement, []).append(report.outcome)
    return report


def requirement_table():
    rows = ["| ID | Requirement | Tests | Passed | Result |", "|---|---|---|---|---|"]
    for requirement, title in requirements():
        outcomes = results.get(requirement, [])
        passed = outcomes.count("passed")
        if not outcomes:
            result = "NOT TESTED"
        elif passed == len(outcomes):
            result = "PASS"
        else:
            result = "FAIL"
        rows.append(f"| {requirement} | {title} | {len(outcomes)} | {passed} | {result} |")
    return "\n".join(rows) + "\n"


def pytest_terminal_summary(terminalreporter):
    table = requirement_table()
    terminalreporter.section("requirements")
    terminalreporter.write(table)
    (ROOT / "build").mkdir(exist_ok=True)
    (ROOT / "build" / "requirements.md").write_text(
        f"### Requirements ({platform.machine()})\n\n{table}")
