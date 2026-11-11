# Roadmap

What v0.1 deliberately does not do, and what comes next. The scope of v0.1 is in
[docs/REQUIREMENTS.md](docs/REQUIREMENTS.md).

## Next features

| Feature | Why |
|---|---|
| Write `SHA256SUMS` on the robot (`rosbagcheck --write-sums`, or a hook in the recorder) | Check 1 only works if someone remembers `sha256sum` after every run. |
| Test-status page over time | One run says "this recording is fine"; a page over many field days shows trends (which sensor drops out most, which robot). |
| Rules per robot (`--expect` picks the file from the recording's metadata) | Different robots have different sensors and rates. |
| ROS 1 `.bag` files | Older recordings still exist; needs a second reader. |
| Check message contents (e.g. no NaN in `/odom`) | A sensor can send on time and still send garbage. |

## Known limitations

- Without `--expect`, check 3 only measures and never fails: a dead sensor is HEALTHY ([#15](https://github.com/AhmadOudeh/rosbagcheck/issues/15)).
- Check 3 reads every message record to get its time. Fine for a few GB; slow for very large runs.
- Check 3 keeps the time of every message (8 bytes each), so its memory grows with the number of
  messages: about 64 MB for 3.6 million messages (1 kHz for one hour).
- Check 3 fails when two files overlap in time. This assumes that ROS 2 split files never overlap,
  which is not yet tested with a real `ros2 bag record` session.
- No live monitoring while recording, no repair of broken files, no GUI.
- Open bugs: [#9](https://github.com/AhmadOudeh/rosbagcheck/issues/9) a FAIL message can show a value equal to the limit (rounding),
  [#10](https://github.com/AhmadOudeh/rosbagcheck/issues/10) the topic table is misaligned for long topic names, [#15](https://github.com/AhmadOudeh/rosbagcheck/issues/15) see above,
  [#16](https://github.com/AhmadOudeh/rosbagcheck/issues/16) some error messages name the wrong cause.

## Harden and refactor

| What | Why |
|---|---|
| Read only the MCAP message index for check 3 | The index has every message time without the message data: much faster on large files. |
| Fuzz tests for the `SHA256SUMS` and rules parsers | Both read files a person wrote by hand; random input finds crashes that hand-written tests miss. |
| Release binaries and a `.deb` package per architecture | Today you build it yourself or use Docker. |
| Run the tests with sanitizers (AddressSanitizer, UndefinedBehaviorSanitizer) in CI | Catches memory bugs in C++ that tests do not see. |
| Tests on files of real size (2 GB+) in a nightly job | NFR tests use 256 MB to keep CI fast. |
