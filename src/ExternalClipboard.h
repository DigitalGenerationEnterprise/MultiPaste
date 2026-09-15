#pragma once

#include <QObject>
#include <QByteArray>

namespace multipaste {

// Common interface for clipboard backends that work while the app has no
// focus. Implementations:
//
//   * KlipperClipboard - Plasma's native clipboard manager over D-Bus
//                        (org.kde.klipper). Preferred on Plasma; no extra
//                        processes and no data-control clients.
//   * WaylandClipboard - wl-copy/wl-paste data-control bridge (fallback when
//                        Klipper is not available on Wayland).
//
// Both push new values via setText() and deliver changed contents through
// the textRead() signal, either because requestText() was called or because
// the backing source notified us (Klipper's clipboardHistoryUpdated signal).
class ExternalClipboard : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    // Re-reads the current selection now and emits textRead() if it changed
    // (and isn't a value we wrote ourselves). Safe to call repeatedly;
    // no-ops while an asynchronous read is in flight.
    virtual void requestText() = 0;

    // Puts `text` on the selection. Implementations must make their own writes
    // invisible to requestText()/signal-driven reads.
    virtual bool setText(const QByteArray &text) = 0;

    // True while `setText` values are pushed through this backend, i.e. the
    // core should not also write through Qt's QClipboard.
    virtual bool ownsWrites() const { return true; }

    QString backendName() const { return m_backendName; }

protected:
    void setBackendName(const QString &name) { m_backendName = name; }

signals:
    void textRead(const QByteArray &text);

private:
    QString m_backendName;
};

} // namespace multipaste