# Running the test suite under ThreadSanitizer

Two tests (`tests/ProxySessionTests.cpp`, `tests/SongBirdAutoCoordinatorStopTests.cpp`) reason about
what a background worker may touch after the object that owns it is gone. This file is how to check
that reasoning with a sanitizer instead of re-reading it.

## Build

```sh
cmake -S . -B build-tsan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TEST=ON \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"
cmake --build build-tsan --parallel "$(nproc)"
```

## Run

Only the two binaries that drive the crash-restart and queued-result paths, to keep the noise down:

```sh
TSAN_OPTIONS="second_deadlock_stack=1" QT_QPA_PLATFORM=offscreen \
  ./build-tsan/tests/songbird_proxy_session_tests 2>&1 | tee tsan.log
TSAN_OPTIONS="second_deadlock_stack=1" QT_QPA_PLATFORM=offscreen \
  ./build-tsan/tests/songbird_auto_coordinator_stop_tests 2>&1 | tee -a tsan.log
grep -c "WARNING: ThreadSanitizer" tsan.log
```

A non-zero count is expected and is *not* a finding yet — read on.

## How to read the output

The guard has to discriminate, so the question is not "were any races reported" but "does any report
name a data member of ours".

- **A report whose `SUMMARY` line names `src/...` is a finding.** That is a race in SongBird's own
  memory, and it is worth an issue.
- **A report whose `SUMMARY` names `libQt5Core.so.5` or `libQt5Network.so.5` is not.** The Qt
  installed by Debian is not built with ThreadSanitizer, so the sanitizer cannot see the mutexes
  inside it and reports the allocator and container traffic behind its own `QWaitCondition`
  destruction, `QHash` node reuse and meta-type tables as races. Every one of those reports has a
  `libQt5*.so.5` symbol in its `SUMMARY` line.

The failure mode this file exists to prevent: a count of warnings from a Qt that was never
instrumented says nothing about our code, and reading it as if it did is how a comment ends up
claiming a race the sanitizer never observed. Check the `SUMMARY` symbol, not the number.
