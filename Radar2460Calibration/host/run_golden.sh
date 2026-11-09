#!/bin/sh
# Golden test of the radar frame parser: builds the sketch against the scripted
# byte streams of host/test_stream.cpp and compares its output with host/golden.txt.
#   host/run_golden.sh            # compare, non-zero exit on difference
#   UPDATE=1 host/run_golden.sh   # rewrite golden.txt after an intended change
set -eu
cd "$(dirname "$0")/.."
CXX="${CXX:-g++}"
OUT_DIR="${OUT_DIR:-$(mktemp -d)}"

"$CXX" -std=c++17 -O2 -Wall -Wextra -Werror -Ihost -I. host/main.cpp host/test_stream.cpp -o "$OUT_DIR/radar-golden"
"$OUT_DIR/radar-golden" > "$OUT_DIR/actual.txt"

if [ "${UPDATE:-0}" = 1 ]; then
    cp "$OUT_DIR/actual.txt" host/golden.txt
    echo "host/golden.txt updated"
elif diff -u host/golden.txt "$OUT_DIR/actual.txt"; then
    echo "golden test OK"
else
    echo "golden test FAILED (UPDATE=1 $0 to accept the new output)" >&2
    exit 1
fi
