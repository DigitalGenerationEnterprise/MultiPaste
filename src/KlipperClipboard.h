#pragma once

#include "ExternalClipboard.h"

#include <QDBusInterface>

namespace multipaste {

// Native clipboard backend using Plasma's Klipper D-Bus service
// (org.kde.klipper / /klipper / org.kde.klipper.klipper).
//
// Klipper is the authoritative selection owner on both Plasma 5 and 6; it
// exposes getClipboardContents() / setClipboardContents() and a
// clipboardHistoryUpdated signal. This means:
//
//  * No process-per-second polling: change notification is push-based.
//  * No data-control clients (wl-copy/wl-paste): reads and writes go
//    straight through the running clipboard daemon.
//  * setText() works while the app has no focus — Klipper owns the
//    selection, not us.
//
// Falls back silently if Klipper is not running (e.g. non-KDE session).
class KlipperClipboard : public ExternalClipboard
{
    Q_OBJECT

public:
    // True when the org.kde.klipper D-Bus service is registered.
    static bool available();

    explicit KlipperClipboard(QObject *parent = nullptr);
    ~KlipperClipboard() override;

    void requestText() override;
    bool setText(const QByteArray &text) override;

private slots:
    void onHistoryUpdated();

private:
    void readNow();

    QDBusInterface m_iface;
    QByteArray m_lastRead;
    QByteArray m_lastWritten;
    bool m_internalWrite = false; // true while the setText call is being made
};

} // namespace multipaste