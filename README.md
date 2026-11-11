# rosbagcheck

rosbagcheck tells you with one command whether a ROS 2 field-test recording can be trusted: copied
completely, closed properly, and every sensor sending the whole time.



https://github.com/user-attachments/assets/a6d17b82-9dc5-4274-b588-18c8e7e57ebb



## Why

After a robot field test, the recordings are copied off the robot with `scp`, often several
gigabytes per run, so the team can analyse what happened. If the copy stopped early, the recorder
was killed, or a sensor went silent, the analysis is wrong, and you may find out only after the
recording on the robot is deleted. rosbagcheck checks this right after the copy, in seconds.

## Who it is for

- **Field engineer**, right after the copy: "Can I delete the recordings on the robot now?"
- **Analyst**, before starting work: "Can I trust this data?"
- **Automation**, e.g. a nightly job over all new uploads: reads the exit code or the JSON report.

## Terms

| Term | Meaning |
|---|---|
| ROS 2 | Robot Operating System 2: the software framework most robots use to pass data between programs. |
| Topic | A named data stream, e.g. `/scan` (lidar) or `/camera/image_raw`. Each sensor sends messages on its topic. |
| Recording (bag) | A folder with everything that was sent on the topics during a test run. |
| MCAP | The file format of ROS 2 recordings (`*.mcap`). A properly closed file ends with a summary and a footer. |
| `SHA256SUMS` | A list of files and their checksums, written by the Linux tool `sha256sum`. Any changed or missing byte changes the checksum. |
| Rate | Messages per second (Hz) on a topic. |
| Gap | The longest time a topic sent nothing. |

## How it works

You give rosbagcheck one or more recording folders. It runs three checks on each, prints a report
(PASS / FAIL / SKIP per check, with the reason for every problem), ends with `verdict HEALTHY` or
`verdict UNHEALTHY`, and sets the [exit code](#exit-codes).

| Check | How | Fails when |
|---|---|---|
| 1 Complete copy | Computes the SHA-256 checksum of every file and compares it with `SHA256SUMS`, written on the robot before the copy. | A listed file is missing or changed, an `.mcap` file is not listed, or `SHA256SUMS` is missing or broken. |
| 2 Properly closed | Reads the end of every `.mcap` file, where a finished recording has its summary and footer. | A file has no valid summary and footer (recorder killed, disk full), or the folder has no `.mcap` files. |
| 3 Sensors alive | Reads the time of every message and computes, per topic: count, rate and longest gap, compared with the limits in your [rules file](#rules-file). | A topic is too slow, silent too long, or missing, or two files overlap in time (the same data twice). Skipped when check 2 fails. |

Without a rules file, check 3 only measures and never fails. rosbagcheck only reads; it never changes
a recording.

## Quick start (Docker)

Works on any machine with [Docker](https://docs.docker.com/get-docker/) (Linux, macOS, Windows with
WSL). Run these in a terminal:

1. Get the code and build the program (a few minutes the first time):

   ```bash
   git clone https://github.com/AhmadOudeh/rosbagcheck.git
   cd rosbagcheck
   docker build -t rosbagcheck .
   ```

2. Make three invented test runs in `demo/` (healthy, cut-off copy, lidar stops):

   ```bash
   scripts/dev.sh python3 examples/make_demo.py demo
   ```

3. Check the healthy one:

   ```bash
   docker run --rm -v "$PWD":/data rosbagcheck --expect examples/rules.yaml demo/run_ok
   ```

   The last line says `verdict HEALTHY`. Now try `demo/run_cut_copy` and `demo/run_lidar_stops`.

`-v "$PWD":/data` makes the current folder visible inside the container, so all paths are relative to
it. To check your own recordings, mount their folder instead:
`docker run --rm -v ~/runs:/data rosbagcheck run_042`.

## Example: after a field test

On the robot, write the checksums when the recording is finished:

```bash
cd ~/recordings/run_042
sha256sum *.mcap > SHA256SUMS
```

On your laptop, copy the run and check it:

```bash
scp -r rover-01:recordings/run_042 ~/runs/
rosbagcheck --expect rules.yaml ~/runs/run_042
```

The demo run where the lidar stopped after 6 of 10 seconds:

```text
recording demo/run_lidar_stops
  PASS  complete copy     1 file(s) match SHA256SUMS
  PASS  properly closed   1 file(s) closed properly
  FAIL  sensors alive     2 problem(s) in 4 topic(s)
        - /scan: rate 6.00 Hz below minimum 8 Hz
        - /scan: silent for 4.000 s starting at t=6.000 s (limit 0.5 s)

  topic                   messages    rate Hz   max gap s  rule                          result
  /camera/image_raw            201      20.00       0.050  >=15 Hz gap<=0.5 s required   PASS
  /diagnostics                   0       0.00      10.000  optional                      SKIP
  /odom                        501      50.00       0.020  >=40 Hz gap<=0.1 s required   PASS
  /scan                         61       6.00       4.000  >=8 Hz gap<=0.5 s required    FAIL

verdict UNHEALTHY
```

A cut-off copy fails checks 1 and 2:

```text
recording demo/run_cut_copy
  FAIL  complete copy     1 problem(s)
        - run_000.mcap: checksum differs (copy incomplete or file changed)
  FAIL  properly closed   1 of 1 file(s) not closed properly
        - run_000.mcap: not closed properly, no valid summary and footer (invalid magic bytes in Footer: 0x000008F59B000000)
  SKIP  sensors alive     skipped: not every file closed properly
verdict UNHEALTHY
```

## Usage

```text
rosbagcheck [--expect RULES.yaml] [--json] FOLDER...
```

| Option | Meaning |
|---|---|
| `FOLDER...` | One or more recording folders, reported in the given order. |
| `--expect RULES.yaml` | Limits per topic for check 3. |
| `--json` | A JSON report instead of text, for scripts. |
| `--help`, `--version` | Help text, version. |

### Exit codes

| Code | Meaning |
|---|---|
| 0 | Every recording is healthy. |
| 1 | At least one recording is unhealthy (the report says why). |
| 2 | rosbagcheck could not check: wrong option, folder not found, broken rules file. |

### Rules file

One entry per topic. Every listed topic is required unless `required: false`. Limits are inclusive
(exactly 8 Hz passes `min_rate_hz: 8`). Unknown keys and wrong values are errors (exit 2), so a typo
cannot silently turn a rule off. Full example: [`examples/rules.yaml`](examples/rules.yaml).

```yaml
topics:
  - topic: /scan
    min_rate_hz: 8     # at least 8 messages per second on average
    max_gap_s: 0.5     # never silent for more than 0.5 s
  - topic: /diagnostics
    required: false    # checked only if it is in the recording
```

### Many runs: which can I delete on the robot?

[`examples/after_copy.sh`](examples/after_copy.sh) checks every run folder and decides by the exit
code alone. It deletes nothing.

```bash
docker run --rm -v "$PWD":/data --entrypoint examples/after_copy.sh rosbagcheck examples/rules.yaml demo/*
```

```text
KEEP  demo/run_cut_copy  -> do not delete; details: rosbagcheck --expect examples/rules.yaml demo/run_cut_copy
KEEP  demo/run_lidar_stops  -> do not delete; details: rosbagcheck --expect examples/rules.yaml demo/run_lidar_stops
OK    demo/run_ok  -> safe to delete on the robot
```

## Build without Docker (Ubuntu 24.04)

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake pkg-config ca-certificates libssl-dev libzstd-dev liblz4-dev \
    nlohmann-json3-dev libyaml-cpp-dev libcli11-dev libgtest-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
sudo cp build/rosbagcheck /usr/local/bin/
rosbagcheck --version
```

The build downloads the MCAP library (pinned version, checked by SHA-256), so it needs internet once.

## Built with

The checks, tests, scripts and docs are written for this project. Not written here:
[MCAP C++ library](https://github.com/foxglove/mcap) (reads the files),
[OpenSSL](https://www.openssl.org/) (SHA-256), [yaml-cpp](https://github.com/jbeder/yaml-cpp),
[nlohmann/json](https://github.com/nlohmann/json), [CLI11](https://github.com/CLIUtils/CLI11),
[GoogleTest](https://github.com/google/googletest), [pytest](https://pytest.org/) and the
[MCAP Python writer](https://pypi.org/project/mcap/) (test data).

## Documentation

- [CONTRIBUTING.md](CONTRIBUTING.md): code structure, development setup, tests, bug reports, design notes
- [ROADMAP.md](ROADMAP.md): next features, known limitations
- [docs/REQUIREMENTS.md](docs/REQUIREMENTS.md): scope and requirements
- [docs/ESSAY.md](docs/ESSAY.md): how and why it was built
- [LICENSE](LICENSE): MIT
