#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CC=${CC:-cc}
TMP_DIR=$(mktemp -d)
TARGET_PID=
cleanup() {
    if [ -n "$TARGET_PID" ]; then
        kill "$TARGET_PID" 2>/dev/null || true
        wait "$TARGET_PID" 2>/dev/null || true
    fi
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT HUP INT TERM

"$CC" -D_GNU_SOURCE -std=c11 -Wall -Wextra -Werror \
    "$ROOT/src/privileged_terminator.c" -o "$TMP_DIR/nebula-process-terminator"
HELPER="$TMP_DIR/nebula-process-terminator"

set +e
"$HELPER" --terminate 1 0 "$$" >/dev/null 2>&1
pid1_status=$?
"$HELPER" --terminate invalid 0 "$$" >/dev/null 2>&1
invalid_status=$?
set -e
[ "$pid1_status" -ne 0 ] || { echo 'FAIL: PID 1 was accepted' >&2; exit 1; }
[ "$invalid_status" -ne 0 ] || { echo 'FAIL: invalid PID was accepted' >&2; exit 1; }
echo 'PASS: helper rejects PID 1 and malformed input'

sleep 30 &
TARGET_PID=$!
START_TICKS=$(awk '{print $22}' "/proc/$TARGET_PID/stat")

set +e
"$HELPER" --terminate "$TARGET_PID" "$((START_TICKS + 1))" "$$" >/dev/null 2>&1
stale_status=$?
set -e
[ "$stale_status" -ne 0 ] || { echo 'FAIL: stale start time was accepted' >&2; exit 1; }
kill -0 "$TARGET_PID" 2>/dev/null || { echo 'FAIL: stale identity test killed target' >&2; exit 1; }
echo 'PASS: helper rejects mismatched process start time'

"$HELPER" --terminate "$TARGET_PID" "$START_TICKS" "$$"
set +e
wait "$TARGET_PID"
wait_status=$?
set -e
TARGET_PID=
[ "$wait_status" -eq 143 ] || { echo "FAIL: expected SIGTERM exit status 143, got $wait_status" >&2; exit 1; }
echo 'PASS: helper sends SIGTERM to matching test process'
