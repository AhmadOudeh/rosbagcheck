"""Writes synthetic ROS 2 recordings (MCAP) for the component tests.

Uses the official mcap Python writer, which is independent of the C++ reader inside rosbagcheck.
All names and data are invented: a robot "rover-01" with three standard ROS topics.
"""
import subprocess

from mcap.writer import CompressionType, Writer

START_NS = 1_700_000_000 * 1_000_000_000  # fixed start time, so every run writes the same data
SECOND_NS = 1_000_000_000
HEALTHY_RATES_HZ = {"/camera/image_raw": 20, "/scan": 10, "/odom": 50}
DURATION_S = 10


def healthy_topics():
    """Log times (ns) per topic: every sensor sends at its rate for the whole recording."""
    return {
        topic: [START_NS + i * SECOND_NS // rate for i in range(DURATION_S * rate + 1)]
        for topic, rate in HEALTHY_RATES_HZ.items()
    }


def write_mcap(path, topics, closed=True, compression="none"):
    """One MCAP file. closed=False is a recorder that was killed: no summary, no footer."""
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
                               sequence=sequence, data=b"{}")
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
    topics = topics or healthy_topics()
    for index in range(files):
        # File k gets every files-th message of each topic: simple, and every file has data.
        part = {topic: times[index::files] for topic, times in topics.items()}
        write_mcap(folder / f"run_{index:03d}.mcap", part, **write_options)
    write_sums(folder)
    return folder
