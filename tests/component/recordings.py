"""Writes synthetic ROS 2 recordings (MCAP) for the component tests.

Uses the official mcap Python writer, which is independent of the C++ reader inside rosbagcheck.
All names and data are invented: a robot "rover-01" with three standard ROS topics.
"""
import subprocess
from pathlib import Path

from mcap.writer import CompressionType, Writer

START_NS = 1_700_000_000 * 1_000_000_000  # fixed start time, so every run writes the same data
SECOND_NS = 1_000_000_000
HEALTHY_RATES_HZ = {"/camera/image_raw": 20, "/scan": 10, "/odom": 50}
DURATION_S = 10
EXAMPLE_RULES = Path(__file__).resolve().parents[2] / "examples" / "rules.yaml"


def periodic(rate_hz, duration_s=DURATION_S, silent=None):
    """Log times (ns) of one sensor sending at rate_hz from 0 to duration_s.
    silent=(3.0, 7.0): no messages strictly between 3 s and 7 s, so the longest gap is 4 s."""
    count = round(duration_s * rate_hz) + 1
    times = [START_NS + round(i * SECOND_NS / rate_hz) for i in range(count)]
    if silent:
        begin, end = (START_NS + round(s * SECOND_NS) for s in silent)
        times = [t for t in times if not begin < t < end]
    return times


def healthy_topics():
    """Log times (ns) per topic: every sensor sends at its rate for the whole recording."""
    return {topic: periodic(rate) for topic, rate in HEALTHY_RATES_HZ.items()}


def write_mcap(path, topics, closed=True, compression="none", payload=b"{}"):
    """One MCAP file. closed=False is a recorder that was killed: no summary, no footer.
    payload: the data of every message (large payloads make large files for the speed tests)."""
    compression_type = {"none": CompressionType.NONE, "zstd": CompressionType.ZSTD, "lz4": CompressionType.LZ4}
    # Small chunks when not closed, so some messages are on disk, as with a real killed recorder.
    with open(path, "wb") as stream:
        writer = Writer(stream, chunk_size=1024 if not closed else 1024 * 1024,
                        compression=compression_type[compression])
        writer.start(profile="ros2", library="rosbagcheck-tests")
        schema = writer.register_schema(name="Opaque", encoding="jsonschema", data=b'{"type":"object"}')
        channels = {
            topic: writer.register_channel(topic=topic, message_encoding="json", schema_id=schema)
            for topic in topics
        }
        messages = sorted((time, topic) for topic, times in topics.items() for time in times)
        for sequence, (time, topic) in enumerate(messages):
            writer.add_message(channel_id=channels[topic], log_time=time, publish_time=time,
                               sequence=sequence, data=payload)
        if closed:
            writer.finish()
    return path


def write_sums(folder):
    """SHA256SUMS written with the real sha256sum, as on the robot."""
    names = sorted(p.name for p in folder.glob("*.mcap"))
    sums = subprocess.run(["sha256sum", *names], cwd=folder, capture_output=True, text=True, check=True)
    (folder / "SHA256SUMS").write_text(sums.stdout)


def make_recording(folder, topics=None, files=1, **write_options):
    """A complete recording folder: `files` MCAP files split by time, plus SHA256SUMS."""
    folder.mkdir(parents=True, exist_ok=True)
    topics = healthy_topics() if topics is None else topics
    # Like ROS 2: file k holds the k-th time slice, so the files follow each other and never overlap.
    all_times = [t for times in topics.values() for t in times] or [START_NS]
    first, last = min(all_times), max(all_times) + 1
    edges = [first + (last - first) * k // files for k in range(files + 1)]
    for index in range(files):
        part = {topic: [t for t in times if edges[index] <= t < edges[index + 1]]
                for topic, times in topics.items()}
        write_mcap(folder / f"run_{index:03d}.mcap", part, **write_options)
    write_sums(folder)
    return folder
