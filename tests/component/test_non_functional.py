"""Non-functional requirements: memory, speed and platform, measured on a 256 MB recording."""
import os
import platform
import subprocess
import time

import pytest

from recordings import EXAMPLE_RULES, healthy_topics, make_recording

SIZE_BYTES = 256 * 1024 * 1024


@pytest.fixture(scope="module")
def big_recording(tmp_path_factory):
    """A healthy recording of at least 256 MB: the normal topics, each message about 330 KB."""
    topics = healthy_topics()
    messages = sum(len(times) for times in topics.values())
    folder = make_recording(tmp_path_factory.mktemp("big") / "run", topics,
                            payload=bytes(SIZE_BYTES // messages + 1))
    assert (folder / "run_000.mcap").stat().st_size >= SIZE_BYTES
    return folder


def measure_run(binary, *args):
    """Runs rosbagcheck once: (exit code, seconds, peak memory in MB)."""
    begin = time.monotonic()
    process = subprocess.Popen([binary, *args], stdout=subprocess.DEVNULL)
    _, status, usage = os.wait4(process.pid, 0)  # like wait(), plus the child's resource usage
    seconds = time.monotonic() - begin
    process.returncode = os.waitstatus_to_exitcode(status)
    return process.returncode, seconds, usage.ru_maxrss / 1024  # ru_maxrss is in KB on Linux


@pytest.fixture(scope="module")
def big_run(request, big_recording):
    """One run of all three checks on the big recording, shared by the tests below."""
    binary = request.config.getoption("--binary")
    code, seconds, memory_mb = measure_run(binary, "--expect", str(EXAMPLE_RULES), str(big_recording))
    print(f"\n256 MB recording: {seconds:.2f} s, peak memory {memory_mb:.0f} MB")
    return code, seconds, memory_mb


@pytest.mark.req("NFR-2")
def test_memory_stays_low_on_a_256_mb_recording(big_run):
    code, _, memory_mb = big_run
    assert code == 0
    assert memory_mb < 64, f"peak memory {memory_mb:.0f} MB"


@pytest.mark.req("NFR-3")
def test_256_mb_recording_is_checked_in_under_10_seconds(big_run):
    code, seconds, _ = big_run
    assert code == 0
    assert seconds < 10, f"took {seconds:.2f} s"


@pytest.mark.req("NFR-4")
def test_runs_on_x86_64_or_arm64():
    """The whole suite runs on both in CI; this records which one this run was."""
    assert platform.machine() in ("x86_64", "aarch64")
