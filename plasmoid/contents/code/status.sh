#!/bin/sh
# MultiPaste Activity widget bridge: dump the app's JSON snapshot to stdout.
# Falls back to a small JSON blob when the app is not running.
SNAP="$HOME/.cache/multipaste/activity.json"
if [ -f "$SNAP" ]; then
    cat "$SNAP"
else
    printf '{"status":"off","items":[],"next":0,"enabled":false,"activity":["MultiPaste is not running"]}\n'
fi
exit 0