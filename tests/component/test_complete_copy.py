"""Check 1, complete copy: the program against folders made with the real `sha256sum`."""
import json
import os
import subprocess

import pytest


def make_copy(folder, sha256sum_args=("run_000.mcap", "run_001.mcap")):
    """A recording folder with two files and a SHA256SUMS written by the real sha256sum."""
    folder.mkdir()
    (folder / "run_000.mcap").write_bytes(os.urandom(200_000))
    (folder / "run_001.mcap").write_bytes(os.urandom(50_000))
    sums = subprocess.run(["sha256sum", *sha256sum_args], cwd=folder, capture_output=True, text=True, check=True)
    (folder / "SHA256SUMS").write_text(sums.stdout)
    return folder


def copy_check(run, folder):
    """Runs rosbagcheck --json and returns the complete_copy result of the one folder."""
    result = run("--json", str(folder))
    return json.loads(result.stdout)["recordings"][0]["checks"]["complete_copy"]


@pytest.mark.req("FR-1")
@pytest.mark.parametrize(
    "sha256sum_args",
    [("run_000.mcap", "run_001.mcap"), ("-b", "run_000.mcap", "run_001.mcap"), ("./run_000.mcap", "./run_001.mcap")],
    ids=["text mode", "binary mode", "./ names"],
)
def test_good_copy_passes(run, tmp_path, sha256sum_args):
    check = copy_check(run, make_copy(tmp_path / "run", sha256sum_args))
    assert check["status"] == "PASS", check
    assert check["problems"] == []


@pytest.mark.req("FR-1", "NFR-5")
def test_cut_off_file_fails_and_names_it(run, tmp_path):
    folder = make_copy(tmp_path / "run")
    with open(folder / "run_000.mcap", "r+b") as f:
        f.truncate(100_000)  # the copy stopped halfway
    check = copy_check(run, folder)
    assert check["status"] == "FAIL"
    assert any("run_000.mcap" in p and "checksum" in p for p in check["problems"]), check


@pytest.mark.req("FR-1", "NFR-5")
def test_missing_file_fails_and_names_it(run, tmp_path):
    folder = make_copy(tmp_path / "run")
    (folder / "run_001.mcap").unlink()
    check = copy_check(run, folder)
    assert check["status"] == "FAIL"
    assert any("run_001.mcap" in p and "missing" in p for p in check["problems"]), check


@pytest.mark.req("FR-1")
def test_unlisted_recording_fails(run, tmp_path):
    folder = make_copy(tmp_path / "run")
    (folder / "run_002.mcap").write_bytes(b"recorded after the checksums were written")
    check = copy_check(run, folder)
    assert check["status"] == "FAIL"
    assert any("run_002.mcap" in p and "not listed" in p for p in check["problems"]), check


@pytest.mark.req("FR-1")
def test_missing_sha256sums_fails(run, tmp_path):
    folder = make_copy(tmp_path / "run")
    (folder / "SHA256SUMS").unlink()
    check = copy_check(run, folder)
    assert check["status"] == "FAIL"
    assert any("SHA256SUMS" in p for p in check["problems"]), check


@pytest.mark.req("FR-1")
def test_empty_sha256sums_fails(run, tmp_path):
    folder = make_copy(tmp_path / "run")
    (folder / "SHA256SUMS").write_text("")
    assert copy_check(run, folder)["status"] == "FAIL"


@pytest.mark.req("FR-1", "NFR-5")
def test_broken_line_fails_and_names_the_line(run, tmp_path):
    folder = make_copy(tmp_path / "run")
    with open(folder / "SHA256SUMS", "a") as f:
        f.write("this is not a checksum line\n")
    check = copy_check(run, folder)
    assert check["status"] == "FAIL"
    assert any("line 3" in p for p in check["problems"]), check


@pytest.mark.req("FR-7")
def test_failed_copy_exits_1(run, tmp_path):
    folder = make_copy(tmp_path / "run")
    (folder / "run_001.mcap").unlink()
    assert run(str(folder)).returncode == 1


@pytest.mark.req("NFR-1")
def test_files_are_unchanged_and_read_only_folder_works(run, tmp_path):
    folder = make_copy(tmp_path / "run")
    before = {p.name: p.read_bytes() for p in folder.iterdir()}
    for p in folder.iterdir():
        p.chmod(0o444)
    folder.chmod(0o555)
    try:
        check = copy_check(run, folder)
    finally:
        folder.chmod(0o755)
    assert check["status"] == "PASS", check
    assert {p.name: p.read_bytes() for p in folder.iterdir()} == before
