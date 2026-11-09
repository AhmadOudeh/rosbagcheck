"""Check 2, properly closed: was the recorder stopped cleanly, or killed (robot switched off, disk full)?"""
import json

import pytest

from recordings import make_recording, write_sums


def closed_check(run, folder):
    """Runs rosbagcheck --json and returns the properly_closed result of the one folder."""
    result = run("--json", str(folder))
    return json.loads(result.stdout)["recordings"][0]["checks"]["properly_closed"]


@pytest.mark.req("FR-2")
@pytest.mark.parametrize("compression", ["none", "zstd", "lz4"])
def test_closed_recording_passes(run, tmp_path, compression):
    check = closed_check(run, make_recording(tmp_path / "run", compression=compression))
    assert check["status"] == "PASS", check


@pytest.mark.req("FR-2")
def test_several_closed_files_pass(run, tmp_path):
    check = closed_check(run, make_recording(tmp_path / "run", files=3))
    assert check["status"] == "PASS", check
    assert check["summary"] == "3 file(s) closed properly"


@pytest.mark.req("FR-2", "NFR-5")
def test_killed_recorder_fails_and_names_the_file(run, tmp_path):
    # The robot wrote its checksums after the recorder died, so check 1 passes: only check 2 sees it.
    check = closed_check(run, make_recording(tmp_path / "run", closed=False))
    assert check["status"] == "FAIL"
    assert check["problems"][0].startswith("run_000.mcap: not closed properly"), check


@pytest.mark.req("FR-2")
def test_only_the_broken_file_of_several_is_named(run, tmp_path):
    folder = make_recording(tmp_path / "run", files=3)
    with open(folder / "run_002.mcap", "r+b") as f:
        f.truncate(f.seek(0, 2) - 100)  # the last 100 bytes (the footer) are lost
    write_sums(folder)
    check = closed_check(run, folder)
    assert check["summary"] == "1 of 3 file(s) not closed properly"
    assert len(check["problems"]) == 1 and check["problems"][0].startswith("run_002.mcap"), check


@pytest.mark.req("FR-1", "FR-2")
def test_cut_off_copy_fails_copy_and_closed(run, tmp_path):
    folder = make_recording(tmp_path / "run")
    with open(folder / "run_000.mcap", "r+b") as f:
        f.truncate(f.seek(0, 2) // 2)  # the copy stopped halfway
    report = json.loads(run("--json", str(folder)).stdout)["recordings"][0]["checks"]
    assert report["complete_copy"]["status"] == "FAIL"
    assert report["properly_closed"]["status"] == "FAIL"


@pytest.mark.req("FR-2", "NFR-5")
@pytest.mark.parametrize("content", [b"", b"this is not a recording"], ids=["empty file", "not MCAP"])
def test_file_that_is_not_mcap_fails_and_names_it(run, tmp_path, content):
    folder = tmp_path / "run"
    folder.mkdir()
    (folder / "run_000.mcap").write_bytes(content)
    write_sums(folder)
    check = closed_check(run, folder)
    assert check["status"] == "FAIL"
    assert check["problems"][0].startswith("run_000.mcap: not a readable MCAP file"), check


@pytest.mark.req("FR-2")
def test_folder_without_recordings_fails(run, tmp_path):
    folder = tmp_path / "run"
    folder.mkdir()
    check = closed_check(run, folder)
    assert check["status"] == "FAIL"
    assert check["problems"] == ["no .mcap files in the folder"]
