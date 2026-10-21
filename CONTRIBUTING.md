# Contributing

Open an issue with the exact ESPHome/ESP-IDF version, board, adapter USB VID/PID and bcdDevice,
masked chip revision, and a redacted log. Distinguish a successful build from device detection,
network traffic, API, OTA, and hardware qualification. Never post secrets, complete firmware
binaries, or unredacted network captures.

Run `python3 tools/run_cpp_tests.py` with Clang installed. It uses AddressSanitizer and
UndefinedBehaviorSanitizer. CI also compiles a pinned ESPHome example with synthetic credentials;
those test images must never be flashed. Keep license notices and add a focused regression test
for changes to parsing, ownership, timeouts, or lifecycle behavior.
