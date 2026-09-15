#pragma once

#include "ExternalClipboard.h"

#include <QByteArray>

class QProcess;

namespace multipaste {

// Out-of-focus clipboard bridge for Wayland sessions.
//
// Qt's Wayland platform only delivers clipboard contents to the window that
// has focus, which a background tray app never has. Plasma (and most
// compositors) expose the server-side "wlr-data-control" protocol instead;
// the `wl-clipboard` tools speak that protocol, so we use them:
//
//   * read:    `timeout 0.6 wl-paste --no-newline`
//   * write:   `wl-copy` fed via stdin (the process stays alive as the
//              selection owner until the next copy replaces it)
//
// This is optional: when the tools are missing the core simply falls back to
// QClipboard (which only works while the app itself has focus). On Plasma,
// KlipperClipboard is preferred over this backend.
class WaylandClipboard : public ExternalClipboard
{
    Q_OBJECT

public:
    // True when running on the Wayland platform and both wl-copy/wl-paste are
    // available (plus `timeout` used to guard a stuck wl-paste).
    static bool available();

    explicit WaylandClipboard(QObject *parent = nullptr);
    ~WaylandClipboard() override;

    // Asks for the current text now; emits textRead() when the (asynchronous)
    // `wl-paste` finishes. Takes a read immediately only if none is in flight.
    void requestText() override;

    // Puts `text` on the clipboard and keeps owning it until replaced.
    // Returns false if wl-copy could not be started (e.g. not supported).
    bool setText(const QByteArray &text) override;

    QByteArray lastText() const
    {
        return m_lastText;
    }

signals:
    void textRead(const QByteArray &text);

private:
    void onReadFinished();
    void cleanupWriters();

    QProcess *m_read = nullptr;
    bool m_readActive = false;
    QByteArray m_lastText;
    QList<QProcess *> m_writers;
};

} // namespace multipaste