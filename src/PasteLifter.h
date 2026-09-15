#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>

struct _XDisplay;

namespace multipaste {

// Injects a normal "Ctrl+V" paste into the focused window.
//
//  * X11 session: synthesizes the key events via XTest (works for every
//    focused application).
//  * Wayland session: there is no protocol for one client to send synthetic
//    input to another client, so we delegate to a helper if one is installed
//    (ydotool/wtype, which use uinput or the virtual-keyboard protocol) and
//    fall back to "manual" mode where the clipboard is loaded and the user
//    presses Ctrl+V themselves.
class PasteLifter : public QObject
{
    Q_OBJECT

public:
    enum class Kind {
        X11,
        Wayland,
        Unknown,
    };

    explicit PasteLifter(QObject *parent = nullptr);
    ~PasteLifter() override;

    static Kind kind();

    // True if we can deliver a fully automatic paste.
    virtual bool isAutomatic() const;

    // Performs the paste. Returns true when key events were injected.
    virtual bool paste();

private:
    bool pasteX11();
    bool pasteWayland();
    bool waylandBackoffActive() const;
    void closeDisplay();

    _XDisplay *m_display = nullptr;

    // After a batch of failed Wayland injection attempts (e.g. the compositor
    // has no virtual-keyboard protocol) stop spawning helpers for a while so a
    // broken environment is not re-probed on every paste.
    int m_consecutiveWaylandFails = 0;
    QElapsedTimer m_lastWaylandFail;
};

} // namespace multipaste