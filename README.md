# MultiPaste

**Copy several things, paste them one by one, in order.**

MultiPaste is a small KDE/Plasma tray utility that remembers every clipboard
copy (full MIME data — text, images, HTML, URLs, files — in the order you
copied it) and pastes the items sequentially on every trigger. If you need to
move three values into a form, a list of URLs into a document, or a handful of
fields out of one app into another: copy them, then paste-paste-paste.

Built with Qt 6 / KDE Frameworks 6. Tested on Ubuntu (KDE Plasma 6, X11 and
Wayland).

## Features

- Captures everything you copy (text, rich text, images, files, binary) via a
  `QClipboard` listener plus a formats-signature poll, with a fingerprint used
  to skip empty payloads and consecutive duplicates.
- FIFO history: paste order always matches copy order, and the next item is
  always shown.
- Two triggers:
  - **ALT + Right Mouse Button** (X11/XWayland, passive grab).
  - **KDE global shortcut** default **Alt+Shift+V** (also `Super+Shift+V`; all
    in *System Settings → Shortcuts*).
- Pasting: **X11** injects `Ctrl+V` and restores the previous clipboard content;
  **Wayland** uses `ydotool`/`wtype` when available, otherwise loads the item
  into your clipboard for a normal `Ctrl+V`.
- Live **Activity panel**: shows the copy order with the next item highlighted
  and a rolling log of everything copied/pasted/restored.
- **Self-repair**: a disorderly kglobalaccel, a lost X11 mouse grab, a missed
  Wayland transfer or a stale lock file are detected and fixed automatically
  (and on demand via the tray's *Repair & Re-sync*).
- **Plasma 6 widget** ("MultiPaste Activity") that mirrors the activity panel
  on your desktop.
- Single instance, per-user, no daemon, no system integration required.

## Requirements

- Linux with KDE Plasma 6 (or any X11/Wayland session; the tray icon prefers
  KDE, `QSystemTrayIcon` works elsewhere).
- Qt **6.5+** with Widgets and DBus, X11 + XTest.
- KDE Frameworks 6 (KGlobalAccel) is **optional**. It enables the global
  keyboard shortcut (`Alt+Shift+V` or `Super+Shift+V`). Without it the app
  still builds and works: the X11 **Alt+Right** mouse trigger, tray, activity
  panel, DBus and Plasma widget are all fully functional - only the keyboard
  shortcut is skipped.
- **Clipboard backends** (in preference order):
  - `klipper` — **native** Plasma clipboard daemon (`org.kde.klipper`). Used
    automatically on KDE Plasma 5/6. Capture is push-based
    (`clipboardHistoryUpdated`), reads/writes go through the running daemon,
    so no extra processes and no `wl-clipboard` data-control clients. Works
    without focus.
  - `wayland` — `wl-clipboard` data-control bridge (`wl-copy` / `wl-paste`),
    used when Klipper is not available on a Wayland session.
  - `qt` — plain Qt `QClipboard` (only works while the app has focus; still
    fully usable on X11 and for non-KDE sessions).
  To force a backend: `multipaste --backend=klipper` (or
  `MULTIPASTE_BACKEND=klipper`); `auto` picks the first available above.

  ```bash
  sudo apt install wl-clipboard   # only needed for the wayland fallback backend
  ```

  Optional, for automatic pasting while a window is focused: `ydotool` or
  `wtype`.

## Install

### Quick per-user install (recommended)

```bash
git clone <your-url>/multipaste.git
cd multipaste
./scripts/install.sh --autostart
```

This builds the app, installs it to `~/.local/bin/multipaste`, installs the
Plasma widget, and enables start-on-login. Then restart plasmashell once to
pick up the widget (`kquitapp6 plasmashell` - it auto-restarts) and add the
widget via *Add Widgets*.

### Build from source

Minimal dependencies (Debian/Ubuntu):

```bash
sudo apt install build-essential cmake qt6-base-dev libx11-dev libxtst-dev
```

Optional — KDE global shortcut on Plasma:

```bash
sudo apt install libkf6globalaccel-dev
```

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build -j"$(nproc)"
./build/multipaste          # run it
ctest --test-dir build      # run the tests
```

Notes per Ubuntu release:

- **Ubuntu 24.10+ / KDE neon / Debian testing+** (KDE Frameworks 6 available):
  install `libkf6globalaccel-dev` for the full keyboard shortcut.
- **Ubuntu 24.04 LTS and older** ship KDE Frameworks 5 (`libkglobalaccel-dev`),
  which this Qt 6 app does not use. That's fine: CMake simply skips the
  optional shortcut and the app still builds and works (use Alt+Right on X11).

### System-wide install

```bash
cmake --build build -j"$(nproc)"
sudo cmake --install build   # /usr/local: binary, .desktop, Plasma widget
```

## Getting started

1. Run MultiPaste — a tray icon appears.
2. Copy a few things (Ctrl+C, context menus, middle-click selection copy all
   work). KDE's own clipboard history (Klipper) keeps everything as usual;
   MultiPaste only watches.
3. Go to the target document and trigger a paste repeatedly:
   - **X11:** hold **Alt** and click **Right**.
   - **Wayland:** press **Alt+Shift+V** (or `Super+Shift+V`).
   - Or use the tray → *Paste Next*, the Activity panel, or the Plasma widget.

The tray menu and the Activity panel show `status line`, the captured items
with the next one highlighted, and the controls:

| Action            | What it does                                        |
|-------------------|-----------------------------------------------------|
| Paste Next        | Injects/presents the next item in copy order        |
| New Sequence      | Restarts from the first item                        |
| Clear Sequence    | Empties the history                                 |
| Enable            | Toggles capturing and pasting                       |
| Activity Panel    | Shows the live item list + activity log window      |
| Repair & Re-sync  | Re-reads the clipboard, re-registers triggers       |
| Start on Login    | Writes `~/.config/autostart/multipaste.desktop`     |

## Fault tolerance / self-repair

- A crashed instance leaves `/tmp/multipaste.lock`; it is treated as stale
  after 5 seconds and removed automatically.
- A 30-second watchdog (toggleable in Settings) re-registers the global
  shortcut if kglobalaccel restarted, re-arms the X11 passive grab if the X
  server reset, and re-reads the clipboard if a Wayland transfer was missed.
- Tray → *Repair & Re-sync* runs all of the above immediately.

## Integrations

### D-Bus

Session bus service `org.multipaste.MultiPaste`, object `/MultiPaste`:

- Properties: `Items` (QStringList), `NextIndex` (int), `Enabled` (bool),
  `Activity` (QStringList), `StatusLine` (QString).
- Methods: `PasteNext()`, `NewSequence()`, `ClearSequence()`,
  `SetEnabled(bool)`, `Refresh()`.
- Signals: `ItemsChanged()`, `PositionChanged()`, `EnabledChanged(bool)`,
  `LogEntry(s)`.

Example:

```bash
busctl --user call org.multipaste.MultiPaste /MultiPaste \
       org.multipaste.MultiPaste PasteNext
```

### Plasma 6 widget

"MultiPaste Activity" shows the copy order, the next item and the live activity
log, with Paste Next / New Seq / Clear buttons. It reads a small JSON snapshot
(`~/.cache/multipaste/activity.json`) the app keeps up to date, so it needs no
extra QML or D-Bus modules and works out of the box.

```bash
./scripts/install-widget.sh    # per-user install
```

then restart plasmashell and *Add Widgets → MultiPaste Activity*.

## How it works (short)

- **Capture:** `QClipboard::dataChanged` plus a gentle 2 s poll, but the real
  work is done by the backend: klipper pushes changes via
  `clipboardHistoryUpdated`, the wayland bridge reads `wl-paste` — either way
  captures happen even without focus. Every offered MIME format is deep-copied;
  a fingerprint skips empties and consecutive duplicates; the app's own
  paste/restore writes are ignored so the sequence never pollutes itself.
- **Paste:** X11 injects a `Ctrl+V` XTest event into the focused window, then
  restores the previous clipboard content (configurable delay; skipped if you
  copied something else during the paste window). Wayland sets the item text
  via klipper (`setClipboardContents`) or `wl-copy`, and injects with `ydotool`
  → `wtype` when present.

## Configuration

Settings live in `~/.config/multipaste/multipaste.conf` (INI) and are edited
via the tray → *Settings* dialog or the Activity panel:

- `maxEntries` — history size (1–200).
- `pollIntervalMs` — clipboard poll interval (default 2000 ms; the klipper
  backend is push-driven, so this only acts as a safety net).
- `restoreDelayMs` / `restoreAfterPaste` — clipboard restore after injected
  pastes.
- `autoRepair` — 30 s self-repair watchdog (default on).
- `activityPanel` — start with the Activity panel open.
- `autostart` — start on login.

The global shortcut is registered with KGlobalAccel (see
`~/.config/kglobalshortcutsrc`, component `multipaste`).

## Troubleshooting

- *Tray icon missing (Wayland):* the app still works; use the global shortcut.
- *Nothing is captured on Wayland:* install `wl-clipboard` (`sudo apt install
  wl-clipboard`); without it the app can only read the clipboard while it has
  focus itself.
- *Nothing pastes on Wayland:* install `ydotool` or `wtype` for auto-injection;
  otherwise you get a tray hint and can `Ctrl+V` yourself.
- *Widget shows a QML error / "Button is not a type":* restart plasmashell
  (`kquitapp6 plasmashell` re-launches it) so the updated widget QML loads.
- *"Already running" after a crash:* wait 5 s for the stale-lock recovery, or
  `rm -f /tmp/multipaste.lock`.
- *Shortcut lost after login:* the self-repair watchdog re-registers it within
  30 s; enable *Repair & Re-sync* in Settings.

## Tests

```bash
QT_QPA_PLATFORM=offscreen ./build/tst_history    # history engine
QT_QPA_PLATFORM=offscreen ./build/tst_core       # capture/FIFO engine
ctest --test-dir build --output-on-failure
```

## Project layout

```
CMakeLists.txt                build + install rules (app, .desktop, widget)
data/multipaste.desktop       launcher entry
src/main.cpp                  entry point, single instance, autostart, watchdog,
                              DBus service + JSON snapshot for the widget
src/MultipasteCore.{h,cpp}    capture/fingerprint/FIFO/restore engine + activity log
src/HistoryItem.{h,cpp}       MIME deep-copy + fingerprint + classification
src/PasteLifter.{h,cpp}       XTest (X11) / ydotool-wtype (Wayland) injection
src/X11MouseTrigger.{h,cpp}   ALT+RMB passive grab (X11 only) + rearm()
src/GlobalShortcutTrigger.{h,cpp}  KGlobalAccel registration + refresh()
src/TrayIcon.{h,cpp}          tray menu (+ activity panel toggle / repair)
src/SettingsDialog.{h,cpp}    settings dialog
src/ActivityPanel.{h,cpp}     live activity/settings panel window
src/MultipasteDbusServer.{h,cpp}   session-bus service org.multipaste.MultiPaste
plasmoid/                     Plasma 6 "MultiPaste Activity" widget package
scripts/install.sh            one-command per-user install
scripts/install-widget.sh     installs the widget into ~/.local/share/plasma/plasmoids
tests/                        Qt Test suite for history + core engine
```

## License

MIT — see [LICENSE](LICENSE).