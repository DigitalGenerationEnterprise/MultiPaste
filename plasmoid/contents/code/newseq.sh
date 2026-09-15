#!/bin/sh
SVC=org.multipaste.MultiPaste
OBJ=/MultiPaste
IFACE=org.multipaste.MultiPaste
if command -v busctl >/dev/null 2>&1; then
    exec busctl --user call "$SVC" "$OBJ" "$IFACE" NewSequence >/dev/null 2>&1
fi
if command -v gdbus >/dev/null 2>&1; then
    exec gdbus call --session --dest "$SVC" --object-path "$OBJ" --method "$IFACE.NewSequence" >/dev/null 2>&1
fi
exit 1