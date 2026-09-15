#!/bin/bash
# Installs the "MultiPaste Activity" Plasma widget for the current user.
#
# Plasma 6 moved the QML DataSource/"executable" engine into
# org.kde.plasma.plasma5support (org.kde.plasma.core no longer provides it), so
# this installer picks the matching QML variant for the installed Plasma and
# installs it as main.qml. Both variants are stored alongside for manual swaps.
#
# After running this, restart plasmashell (kquitapp6 plasmashell) or just log
# out/in, then add the widget via "Add Widgets".
set -euo pipefail

SRC="$(cd "$(dirname "$0")/.." && pwd)/plasmoid"
DEST="${PLASMOID_DIR:-$HOME/.local/share/plasma/plasmoids/org.multipaste.activity}"

# Plasma 6 = org.kde.plasma.plasma5support module exists on the system.
plasma6=false
for p in \
    /usr/lib/*/qt6/qml/org/kde/plasma/plasma5support \
    /usr/lib/qt6/qml/org/kde/plasma/plasma5support \
    /usr/share/qt6/qml/org/kde/plasma/plasma5support; do
    if [ -d "$p" ]; then plasma6=true; break; fi
done

mkdir -p "$DEST/contents/ui" "$DEST/contents/code"
cp "$SRC/metadata.json" "$DEST/"
# Both variants always stored (main.plasma5.qml / main.plasma6.qml).
cp "$SRC/contents/ui/main.qml" "$DEST/contents/ui/main.plasma6.qml"
cp "$SRC/contents/ui/main.plasma5.qml" "$DEST/contents/ui/"
if $plasma6; then
    cp "$SRC/contents/ui/main.qml" "$DEST/contents/ui/main.qml"
else
    cp "$SRC/contents/ui/main.plasma5.qml" "$DEST/contents/ui/main.qml"
fi
cp "$SRC/contents/code/"*.sh "$DEST/contents/code/"
chmod +x "$DEST/contents/code/"*.sh

echo "Installed widget ($($plasma6 && echo Plasma 6 || echo Plasma 5) variant) to:"
echo "  $DEST"
echo
echo "Next: restart plasmashell, then right-click the desktop ->"
echo "  Add Widgets -> MultiPaste Activity."