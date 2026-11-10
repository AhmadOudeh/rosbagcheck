"""Writes three invented field-test runs to try rosbagcheck without a robot.

    python3 examples/make_demo.py demo

demo/run_ok           healthy
demo/run_cut_copy     the copy stopped early: last 100 bytes missing
demo/run_lidar_stops  /scan goes silent after 6 of 10 seconds
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tests" / "component"))
from recordings import healthy_topics, make_recording, periodic  # noqa: E402

out = Path(sys.argv[1] if len(sys.argv) > 1 else "demo")

make_recording(out / "run_ok")

cut = make_recording(out / "run_cut_copy")
with open(cut / "run_000.mcap", "r+b") as f:
    f.truncate(f.seek(0, 2) - 100)

topics = healthy_topics()
topics["/scan"] = periodic(10, duration_s=6)
make_recording(out / "run_lidar_stops", topics)

print(f"wrote 3 runs to {out}/")
