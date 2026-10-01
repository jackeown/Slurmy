#!/usr/bin/env bash
# Bash's built-in timer needs no GNU time package on the compute node.
# Keep solver/limiter diagnostics separate from the timing record.
TIMEFORMAT='%R %U %S'
{ time bash "$CALL_DIR/limiter.sh" 2>> "$CONTROLLER_LOG"; } 2> "$TIMING_LOG"
