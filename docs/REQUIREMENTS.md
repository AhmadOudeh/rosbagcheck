# Scope and requirements (v0.1)

## Problem

After a robotics field test, ROS 2 recordings are copied off the robot (often several gigabytes per
run) so the team can analyse what happened. Before anyone spends a day on that analysis, three
questions need an answer:

1. Did the copy arrive complete and unchanged?
2. Was the recording closed properly, or was the recorder killed (robot switched off, disk full)?
3. Did every sensor keep sending, or did one go silent for a while?

rosbagcheck answers all three with one command and an exit code.

## Users

- **Field engineer**, right after copying recordings: "Can I delete them on the robot now?"
- **Analyst**, before starting work on a recording: "Can I trust this data?"
- **Automation**: a script or pipeline that processes many recordings without a person watching
  (e.g. a nightly job that checks every new upload). It reads the exit code or `--json` and decides
  on its own whether to continue.

## Worst case

A false HEALTHY: a broken recording reported as healthy, because the original on the robot may
already be deleted and nobody notices. Second worst are false alarms: if they happen often, people
stop trusting the tool. So every problem must be detected, and every FAIL must say exactly why.

## In scope (v0.1)

- ROS 2 recordings in MCAP format (`*.mcap`), one or more files per folder.
- A `SHA256SUMS` file as written by `sha256sum`.
- An optional rules file (YAML) with limits per topic.
- Linux command line: report on stdout, errors on stderr, exit code.

## Not in scope (v0.1)

ROS 1 `.bag` files · checking message contents · live monitoring while recording · repairing files ·
a GUI · creating `SHA256SUMS` (use `sha256sum`) · a dashboard of results over time.

## Functional requirements

| ID | Requirement | Why |
|---|---|---|
| FR-1 | **Complete copy.** Every file listed in `SHA256SUMS` exists and has the listed checksum. Fails on a missing or broken `SHA256SUMS`, a missing or changed file, and an `.mcap` file that is not listed. | A cut-off or missing file is the most common copy problem. |
| FR-2 | **Properly closed.** Every `.mcap` file has its summary and footer. A folder without `.mcap` files fails. | If the recorder was killed, the file ends in the middle. |
| FR-3 | **Sensors alive.** Per topic: message count, rate and longest gap (including recording start and end), compared with the rules. Fails when the recording has no messages or when two files overlap in time (the same data twice). Skipped when FR-2 fails. | A silent sensor makes the analysis wrong. |
| FR-4 | **Rules file.** Per topic `min_rate_hz`, `max_gap_s`, `required`. Limits are inclusive. Unknown keys and wrong values are rejected. | A typo must not silently turn off a rule. |
| FR-5 | **Readable report.** PASS / FAIL / SKIP per check, every problem with its reason. | The field engineer decides in seconds. |
| FR-6 | **JSON report** with `--json`. | For scripts and pipelines. |
| FR-7 | **Exit codes.** 0 = all healthy, 1 = at least one unhealthy, 2 = usage or input error. 2 wins over 1. | Scripts decide without reading text. |
| FR-8 | **Several folders** in one run, reported in the given order. A missing folder is named on stderr and does not stop the others. | A whole field day at once. |

## Non-functional requirements

| ID | Requirement | How it is proven |
|---|---|---|
| NFR-1 | **Read-only.** rosbagcheck never changes a recording. | Files are byte-identical after a run; works on a write-protected folder. |
| NFR-2 | **Low memory.** Files are streamed: message data is never kept, only the time of each message (8 bytes). Memory does not grow with the size of the messages. | A 256 MB recording uses under 64 MB. |
| NFR-3 | **Fast enough to run after every copy** (a typical run is about 2 GB). | A 256 MB recording is checked in under 10 s on a standard GitHub runner. |
| NFR-4 | **Linux on x86-64 and arm64** (robot computers such as NVIDIA Jetson are arm64). | All tests pass on Ubuntu 24.04 on both in CI. Other systems use the Docker image. |
| NFR-5 | **Easy to understand.** `--help` explains the checks and exit codes; every problem names the file, line or topic and the reason. | Component tests check the messages. |

The limits are generous on purpose, so a slow runner does not fail a test by accident, but a real
regression (e.g. reading a whole file into memory) still does.

## Definition of done (v0.1)

Every requirement has at least one passing test, CI is green on both architectures, and someone new
to the project can build and run it from the README alone.
