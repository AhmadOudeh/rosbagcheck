# Contributing to rosbagcheck

Thanks for helping. You can:

- **Report a bug**: see [Reporting a bug](#reporting-a-bug).
- **Propose a feature**: open an issue first and describe the user problem. Check
  [ROADMAP.md](ROADMAP.md) and the scope in [docs/REQUIREMENTS.md](docs/REQUIREMENTS.md).
- **Send a fix or a feature**: see [Making a change](#making-a-change).

What the tool does and how to use it: [README.md](README.md).

## How the code is organised

```text
FOLDER... ──> main.cpp ──> 1 checksum.cpp   complete copy
  --expect      │          2 closed.cpp     properly closed
  rules.yaml    │          3 sensors.cpp    sensors alive (limits from rules.cpp)
                └────────> report.cpp ──> text or JSON on stdout ──> exit code 0 / 1 / 2
```

Problems in a recording go into the report (exit 1). Problems with the input (bad option, missing
folder, broken rules file) go to stderr (exit 2).

| Path | What it does |
|---|---|
| `src/main.cpp` | Command line (CLI11), checks all input first, runs the checks per folder, sets the exit code. |
| `src/checksum.*` | Check 1: parses `SHA256SUMS`, hashes each file in a stream (OpenSSL), compares. |
| `src/closed.*` | Check 2: lists the `.mcap` files, asks the MCAP library for summary and footer. |
| `src/mcap_impl.cpp` | Compiles the MCAP library (header-only) exactly once. |
| `src/sensors.*` | Check 3: reads message times per topic, measures rate and longest gap, judges them against the rules. |
| `src/rules.*` | Reads and validates the rules file (yaml-cpp). |
| `src/report.*` | Result types (`PASS` / `FAIL` / `SKIP`) and the text and JSON output. |
| `tests/unit/` | C++ unit tests (GoogleTest), one file per `src` module. |
| `tests/component/` | Python component tests (pytest): run the real binary on generated recordings. |
| `tests/component/recordings.py` | Writes synthetic MCAP recordings with the official Python writer. |
| `examples/` | Rules example, demo data generator, `after_copy.sh`. |
| `docs/REQUIREMENTS.md` | Scope and requirements (FR-x, NFR-x); tests refer to these IDs. |

Everything except `main.cpp` is one static library, `rosbagcheck_core`, linked by both the program
and the unit tests. Parsing and judging are pure functions (text or numbers in, result out), so they
are unit-tested without files.

## Development setup

You need Linux or macOS with Docker and git. The dev container is Ubuntu 24.04 with every compiler,
library and Python package, the same image CI uses, so "works on my machine" means "works in CI".

```bash
git clone https://github.com/AhmadOudeh/rosbagcheck.git
cd rosbagcheck
scripts/dev.sh build    # compile into build/
scripts/dev.sh test     # unit tests, then component tests
scripts/dev.sh bash     # a shell inside the container; exit with: exit
```

The repo folder is mounted into the container, so edit files on your machine and rerun.

## Build and run the tests

```bash
scripts/dev.sh build                                            # after every C++ change
scripts/dev.sh ctest --test-dir build --output-on-failure       # all unit tests
scripts/dev.sh build/unit_tests --gtest_filter='ParseSums*'     # some unit tests
scripts/dev.sh pytest                                           # all component tests
scripts/dev.sh pytest tests/component/test_complete_copy.py -k cut_off   # one component test
```

At the end, pytest prints a requirement table: one row per requirement with its tests and result
(`PASS`, `FAIL`, `NOT TESTED`). It is also written to `build/requirements.md` and shown on every CI
run page.

## How we test

Tests follow risk. The worst failure is a **false HEALTHY**: a broken recording reported as healthy,
because the original on the robot may already be deleted. So every way a recording can be broken
has a test that expects FAIL, and every FAIL must name the file or topic and the reason. Second
worst are false alarms, so valid recordings (both `sha256sum` modes, several files, compression)
must PASS.

| Level | Tests | Use it for |
|---|---|---|
| Unit (C++, GoogleTest) | Pure logic: `SHA256SUMS` parsing, SHA-256, rules validation, rate and gap, limits | Edge cases and boundaries: exact limit, empty input, malformed lines. Fast, no files. |
| Component (Python, pytest) | The built binary from the outside: folders, exit codes, text, JSON, speed, memory | What the user sees. Test data comes from the independent MCAP Python writer, not from our C++ code. |

Every component test names the requirement it verifies:

```python
@pytest.mark.req("FR-1")
def test_cut_off_file_fails_and_names_it(run, tmp_path):
```

`--strict-markers` makes a typo in a marker an error. A new requirement needs at least one test,
otherwise it shows `NOT TESTED`.

## Reporting a bug

Open an issue with the [bug report template](.github/ISSUE_TEMPLATE/bug_report.md): requirement,
severity, priority, environment, steps to reproduce, actual, expected, evidence. Use invented test
data only, never real recordings.

**Severity** = how bad the effect is for the user when it happens.

| Severity | Meaning |
|---|---|
| Critical | Data loss, or a false HEALTHY that makes someone delete the only good copy. |
| High | A wrong verdict or missing results in a normal workflow (false HEALTHY on a rule, false UNHEALTHY on a valid copy, no output for a batch). |
| Medium | A crash or wrong verdict that needs unusual input to trigger. |
| Low | Cosmetic problems, developer tooling, enhancements. |

**Priority** = how soon to fix it, given severity, how often it happens and the cost of the fix.
They usually match, but can differ: a severe bug in a rarely used option may get a lower priority.
Write one line of reason for each.

Labels: `bug`, `severity: <level>`, `priority: <level>`.

## Making a change

1. **Branch** from an up-to-date `main`: `feature/<name>`, `fix/<issue number>` or `docs/<name>`.
2. **Commit** one logical change per commit, with an imperative subject (`Add SHA256SUMS parser`,
   not `added stuff`). Add a body only when it explains something the diff cannot, e.g. the cause of
   a bug. Every commit builds and passes the tests.
   - **Feature**: tests and code in the same commit.
   - **Bug fix**: first a commit with a test that fails and shows the bug, then the fix. The failing
     test proves the bug existed and stops it from coming back.
3. **Check** before pushing: build, run all tests, and read your own diff, new files included.
4. **Open a pull request** and fill in the [template](.github/pull_request_template.md): what, why,
   how tested, related issue (`Fixes #12` closes the issue on merge).
5. **CI** builds and tests on Ubuntu 24.04 on x86-64 and arm64. `main` is protected: both jobs must
   be green and the branch up to date before merging.
6. **Merge** with a merge commit (no squash), so the test-first commits stay visible.

This is a one-person project, so CI is the required gate and the author reviews their own diff. With
more maintainers, every pull request would also need one approving review.

## Design notes

Short records of the main decisions: what was chosen, the alternative, and why.

| Decision | Alternative | Why |
|---|---|---|
| C++17 | C++20 | Everything needed (`std::filesystem`, `std::optional`) is in C++17, and every compiler on robot computers supports it. |
| CMake | Make, Bazel | The standard for C++ on Linux; finds system libraries and downloads MCAP with `FetchContent`. |
| Official MCAP C++ library, pinned version + SHA-256 | Read the bytes ourselves, or vendor a copy | Parsing the format by hand is where bugs hide. Pinning with a hash makes builds repeatable and tamper-proof without a copy in the repo. |
| OpenSSL for SHA-256 | Own SHA-256 | Tested and fast; writing crypto yourself is a classic mistake. |
| yaml-cpp, nlohmann/json, CLI11 | Hand-written parsing | Small, common libraries in Ubuntu; less own code to test. |
| Rate = (messages − 1) / (recording end − recording start), recording = all topics | Per topic, from its own first to last message | A sensor that dies early must fail the rate rule; measured per topic it would still look fast. |
| Gaps include recording start and end | Only gaps between messages | A sensor that starts late or stops early is silent for that time. |
| Rules: unknown keys and bad values are errors | Ignore them | A typo (`min_rate: 8`) must not silently turn a rule off: that would be a false HEALTHY. |
| Read-only, no repair | Repair cut-off files | Changing evidence is risky; the tool decides, people act. |
| Component tests in Python with the MCAP Python writer | Test data written by our C++ code | An independent writer catches mistakes that our reader and writer would share. |
| Docker dev image = CI image | Install tools on each machine | Same compiler and libraries everywhere; works on macOS too. |
| CI on x86-64 and arm64 | x86-64 only | Robot computers (e.g. NVIDIA Jetson) are arm64. |
