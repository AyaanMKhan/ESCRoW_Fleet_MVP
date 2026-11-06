#!/bin/bash
# Usage: lock.sh <owner> <command...>  Runs the command while holding the shared editor/git lock.
D="$(dirname "$0")/editor.lock"; owner="$1"; shift
t=0; until mkdir "$D" 2>/dev/null; do sleep 5; t=$((t+5)); if [ $t -ge 1500 ]; then echo "LOCK TIMEOUT held by $(cat "$D/owner" 2>/dev/null)"; exit 75; fi; done
echo "$owner $(date +%H:%M:%S)" > "$D/owner"; trap 'rm -rf "$D"' EXIT
"$@"
