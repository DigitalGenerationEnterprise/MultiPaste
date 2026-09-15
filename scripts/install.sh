#!/bin/bash
# One-command per-user install for MultiPaste:
#   ./install.sh            build (if needed) and install to ~/.local/bin
#   ./install.sh --no-widget   skip installing the Plasma widget
#   ./install.sh --autostart   also enable "start on login"
set -euo pipefail

SRC="$(cd "$(dirname "$0")/.." && pwd)"
DEST_BIN="${DEST_BIN:-$HOME/.local/bin}"
BUILD_DIR="${BUILD_DIR:-$SRC/build}"

AUTOSTART=0
NO_WIDGET=0
for arg in "$@"; do
    case "$arg" in
        --autostart) AUTOSTART=1 ;;
        --no-widget) NO_WIDGET=1 ;;
        --help|-h)
            echo "usage: $0 [--autostart] [--no-widget]"; exit 0 ;;
        *) echo "unknown option: $arg"; exit 1 ;;
    esac
done

echo "==> Building MultiPaste (Release)..."
cmake -S "$SRC" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$DEST_BIN/.." >/dev/null
cmake --build "$BUILD_DIR" -j"$(nproc)" >/dev/null

echo "==> Installing binary to $DEST_BIN..."
mkdir -p "$DEST_BIN"
install -Dm755 "$BUILD_DIR/multipaste" "$DEST_BIN/multipaste"

if [ "$AUTOSTART" = "1" ]; then
    echo "==> Enabling autostart..."
    mkdir -p "$HOME/.config/autostart"
    cat > "$HOME/.config/autostart/multipaste.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=MultiPaste
Comment=Sequential multi-copy paste utility
Exec=$DEST_BIN/multipaste
X-GNOME-Autostart-enabled=true
Terminal=false
EOF
    # The app also keeps this in sync via its tray menu / settings.
    mkdir -p "$HOME/.config/multipaste"
    cat > "$HOME/.config/multipaste/multipaste.conf" <<EOF
[general]
autostart=true
EOF
fi

if [ "$NO_WIDGET" = "0" ]; then
    "$SRC/scripts/install-widget.sh"
fi

echo
echo "MultiPaste installed. Run it with:  $DEST_BIN/multipaste"
echo "Autostart: enabled in $HOME/.config/autostart/multipaste.desktop (if --autostart)"
echo "Plasma widget: restart plasmashell then Add Widgets -> \"MultiPaste Activity\"."