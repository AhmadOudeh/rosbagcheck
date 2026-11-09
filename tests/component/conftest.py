"""Shared helpers: every component test runs the real rosbagcheck program."""
import subprocess

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
