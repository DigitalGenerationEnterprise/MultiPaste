#!/bin/sh
# MultiPaste Activity widget bridge: ask the running app to paste next.
# Uses busctl (systemd) or gdbus (glib) — one of them is present on any
# Plasma 6 system. Exits 0 on success.
SVC=org.multipaste.MultiPaste
OBJ=/MultiPaste
IFACE=org.multipaste.MultiPaste

if command -v busctl >/dev/null 2>&1; then
    busctl --user call "$SVC" "$OBJ" "$IFACE" PasteNext >/dev/null 2>&1
    exit $?
fi
if command -v gdbus >/dev/null 2>&1; then
    gdbus call --session --dest "$SVC" --object-path "$OBJ" --method "$IFACE.PasteNext" >/dev/null 2>&1
    exit $?
fi
exit 1