# Essay

## The project: what I built and why

After robotics field tests I copy the recordings off the robot, often gigabytes over weak Wi-Fi. If the
copy is incomplete or the recorder was killed, every analysis that follows is wasted, and by then the
original on the robot may be gone. rosbagcheck is a Linux command-line tool in C++17 that tells me
whether a copied ROS 2 recording can be trusted. The README and its short video show what it checks
and how to use it.

Before writing any code, I defined the worst case: a broken recording reported as HEALTHY. I checked
every later decision against it.

## The organization: how I worked

I worked in five steps.

1. **Requirements before code.** The first pull request held only the requirements, each with an ID,
   and a list of what is out of scope.
2. **One change per pull request.** Every change gets its own branch and a pull request with a short
   description. `main` accepts it only when CI passes on x86-64 and arm64.
3. **Testing by risk.** Most tests try to trick the tool into saying HEALTHY when it must not. The rest
   check that valid recordings pass, because people ignore a tool that raises false alarms. C++ unit
   tests (GoogleTest) cover the logic. Python component tests (pytest) run the real binary from the
   outside. The test recordings come from the official MCAP Python writer, not from the C++ code, so a
   mistake shared by reader and writer can't hide. Each component test names its requirement, and CI
   shows PASS, FAIL or NOT TESTED for every requirement.
4. **Finding defects.** Passing tests only prove the cases I thought of. So I attacked my own
   requirements with boundary values and one bad input among many, then ran a 30-minute exploratory
   session as a field engineer. It found a false HEALTHY that every automated test had missed: a copy
   of a file in the folder doubled the message count, and a 5 Hz lidar passed an 8 Hz rule.
5. **Triage.** The findings became issues, each with a severity, a priority and a reason for both. I
   fixed the high-severity bugs that had a clear fix (#8, #14), test-first: one commit with a failing
   test, then one with the fix. #15 is high severity too, but it needs a design decision first. The
   rest stay open and are listed in the ROADMAP. Severity and priority don't always match: a long topic
   name that breaks the table (#10) is only cosmetic, yet real topic names are often that long.

The code is a small library in `src/` with a thin `main.cpp`. The decision logic is kept apart from
file access, so it can be tested without files. The README is for users, CONTRIBUTING for developers,
and the ROADMAP lists what was left out.

## Technological decisions and trade-offs

For each choice I asked one question: does it prevent defects, or make them easier to reproduce?

| Decision | Instead of | Why | Cost |
|---|---|---|---|
| C++17 | C++20 | ROS 2 Humble and Jazzy are written for C++17, so the compilers on robot computers support it. Nothing I use needs C++20. | No newer language features |
| CMake | Make, Bazel | The standard build tool for C++ on Linux. It finds the system libraries and downloads the pinned MCAP library. | More setup than a Makefile |
| Official MCAP library, pinned by version and SHA-256 in CMake | My own parser | Hand-written parsers for binary formats are a classic source of bugs. | A dependency I don't control |
| Rate over the whole recording | Rate over the time the sensor was active | A sensor that dies early must fail (example below). | A sensor that starts late also looks slower |
| One Docker image for development and CI | Tools on each machine | A bug report only helps if someone else can reproduce it. | Docker is needed to run the tests the way CI does |

**Why the whole recording.** A lidar must send at least 8 messages per second. In a 10-second
recording it dies after 6 seconds, having sent about 60 messages.

- Over the whole recording: 60 ÷ 10 s = **6 per second → FAIL**, which is right.
- Over only the 6 seconds it was active: 60 ÷ 6 s = **10 per second → PASS**, and the dead lidar goes unnoticed.

**Third-party code.** These libraries are not mine: the official MCAP C++ library, OpenSSL (SHA-256),
zstd, lz4, yaml-cpp, nlohmann/json, CLI11, GoogleTest and pytest. The code in `src/` and the tests were
written for this project; who did what is below.

## Reflection

**What was new for me.** Before this project I had not done formal software testing. Test levels,
risk-based testing, severity versus priority and tracing tests to requirements were all new to me. I
also rarely write C++, so I built this project with an AI coding assistant (Claude Code). It wrote the
code; I directed and checked it. I set the scope and requirements, decided what to test, read every
change, ran every test myself, ran the exploratory session and triaged the findings. I am responsible
for everything in this repository.

**What I would do differently.**
- **Check requirements against the worst case, not only the code.** #15 isn't a coding mistake. My
  requirement says "compared with the rules", so without rules a dead sensor shows PASS.
- **Make test data realistic early.** My generator split recordings into files in a way ROS 2 never
  does, and I only noticed while fixing #14. Next time I would test with the output of a real
  `ros2 bag record` session.
