#pragma once

#include "HistoryItem.h"

#include <QClipboard>
#include <QElapsedTimer>
#include <QMimeData>
#include <QObject>
#include <QStringList>

#include <vector>

class QTimer;

namespace multipaste {

class PasteLifter;
class ExternalClipboard;

// Backend selection for out-of-focus clipboard access. Auto prefers Klipper
// (native Plasma clipboard daemon), then the Wayland data-control bridge,
// then plain QClipboard.
enum class ClipboardBackend { Auto, Klipper, Wayland, Qt };

// Owns clipboard monitoring, the copy-order sequence state machine, and the
// "set next item -> paste -> restore previous clipboard" operation.
//
// Important guarantees:
//   * never records the temporary clipboard writes MultiPaste makes itself
//     (guarded by an in-call flag AND a ring of recent self-fingerprints)
//   * never touches the X11 "selection" / middle-click clipboard
//   * restores the user's previous clipboard after an automatic paste
class MultipasteCore : public QObject
{
    Q_OBJECT

public:
    explicit MultipasteCore(QObject *parent = nullptr);
    ~MultipasteCore() override;

    void start();
    void stop();

    // Which backend to use for out-of-focus clipboard access. Must be called
    // before start(). Auto picks the first available: Klipper, Wayland bridge,
    // Qt QClipboard.
    void setBackend(ClipboardBackend b);
    ClipboardBackend backend() const
    {
        return m_backend;
    }
    // Name used by the running backend ("klipper", "wayland" or "qt"),
    // for logging and diagnostics.
    QString backendName() const;

    bool enabled() const
    {
        return m_enabled;
    }
    void setEnabled(bool b);

    int maxEntries() const
    {
        return m_maxEntries;
    }
    void setMaxEntries(int n);
    // Copies whose plain text exceeds this length are skipped (paste-queue
    // hygiene: terminal dumps / long chat exports are noise). 0 = keep all.
    int maxCaptureChars() const { return m_maxCaptureChars; }
    void setMaxCaptureChars(int n) { m_maxCaptureChars = qMax(0, n); }

    int pollIntervalMs() const
    {
        return m_pollIntervalMs;
    }
    void setPollIntervalMs(int ms);

    bool restoreAfterPaste() const
    {
        return m_restoreAfterPaste;
    }
    void setRestoreAfterPaste(bool b);

    int restoreDelayMs() const
    {
        return m_restoreDelayMs;
    }
    void setRestoreDelayMs(int ms);

    int count() const
    {
        return m_items.size();
    }

    // Index (0-based) of the next item that would be pasted; -1 when empty.
    int nextIndex() const;

    HistoryItem *itemAt(int i);
    const HistoryItem *itemAt(int i) const;

    // Descriptions in copy order (for menus, the activity panel and DBus).
    QStringList descriptions() const;

    // Ring buffer of recent copy/paste/restore events (newest first) for the
    // activity panel and the Plasma widget.
    QStringList activityLog() const;

    QString statusLine() const;   // "4 items", "disabled"
    QString positionLine() const; // "2 / 4"

    bool isDisabled() const
    {
        return !m_enabled;
    }

    // Optional provider of the real paste injection (kept out of the core so
    // the core stays unit-testable). If unset or returning false, the item is
    // loaded to the clipboard and the user pastes manually.
    void setPasteLifter(PasteLifter *lifter)
    {
        m_lifter = lifter;
    }

    // Main action: put the next sequence item on the clipboard, paste it into
    // the focused application, then restore the previous clipboard.
    void pasteNext();

    // Keep all captured items but restart the paste position at the first one.
    void newSequence();

    // Forget everything.
    void clearSequence();

    // Self-repair: re-read the current clipboard (helps if a Wayland transfer
    // was missed) and clear a stuck empty-retry state.
    void refreshFromClipboard();

    // Ignore the pre-existing clipboard content on startup when it matches
    // what a previous MultiPaste run left behind (persisted by the app as the
    // SHA-256 of the last external text). The guard stays armed while repeated
    // reads still match the seed, so re-emits cannot fake a fresh entry.
    void seedFromFingerprint(const QByteArray &fingerprint)
    {
        m_seedFingerprint = fingerprint;
    }

    // SHA-256 hex of the last text read/written through the external backend,
    // so main() can persist it across restarts. Empty when the Qt path is
    // in use (there the self-write ring already covers a single run).
    QByteArray lastExternalTextFingerprintHex() const;

    // Log is emitted for the activity panel / Plasma widget.
    void setActivityLogSize(int n);

signals:
    void itemsChanged();
    void positionChanged();
    void enabledChanged(bool enabled);

    // Append one line to the activity feed ("COPY  <desc>", "PASTE  <desc>  …").
    void logEntry(const QString &line);

    // Emitted when there is no way to inject the paste automatically
    // (Wayland without a helper tool). The clipboard already holds the item;
    // the user just presses Ctrl+V.
    void manualPasteHint(const QString &description);

    void pastePerformed(const QString &description);
    void errorReported(const QString &message);

private:
    void onClipboardChanged();
    void onPollTick();
    void createExternalBackend();

    void tryCapture();
    void acceptPayload(const QMimeData *md);

    // Out-of-focus clipboard path (Klipper D-Bus / Wayland bridge).
    void requestExternalText();
    void onExternalText(const QByteArray &text);
    bool usesExternalClipboard() const
    {
        return m_external != nullptr;
    }
    QByteArray currentClipboardText() const;

    void setClipboardIgnoring(QMimeData *md);
    void markSelfWrite(QMimeData *md);
    bool isSelfContent(const QByteArray &fingerprint) const;
    void prune();

    void doRestoreAfterPaste(const QByteArray &pastedFingerprint,
                             const QString &description);

    void appendLog(const QString &line);

    QClipboard *m_clipboard = nullptr;
    QTimer *m_pollTimer = nullptr;
    bool m_emptyRetryPending = false;
    ExternalClipboard *m_external = nullptr; // created per backend choice
    ClipboardBackend m_backend = ClipboardBackend::Auto;
    QByteArray m_lastExternalText; // last text read from the external source
    bool m_externalReadPending = false;
    QByteArray m_seedFingerprint; // launch-time content to ignore exactly once

    struct SelfMark;
    std::vector<HistoryItem> m_items; // copy order, oldest first
    int m_next = 0; // index of the NEXT item to paste
    QMimeData *m_lastForeignCopy = nullptr; // owning, most recent user copy
    PasteLifter *m_lifter = nullptr;        // non-owning

    bool m_enabled = true;
    int m_maxEntries = 30;
    int m_maxCaptureChars = 4000;
    int m_pollIntervalMs = 2000;
    bool m_restoreAfterPaste = true;
    int m_restoreDelayMs = 200;
    int m_activityLogSize = 500;

    bool m_running = false;

    // Activity feed (newest last; presented reversed to readers).
    QStringList m_activityLog;

    // Self-write suppression.
    bool m_insideSelfWrite = false;
    struct SelfMark {
        QByteArray fingerprint;
        QElapsedTimer at;
    };
    QList<SelfMark> m_recentSelfMarks; // ring, most recent last
    QString m_lastForeignFormatsSignature;
};

} // namespace multipaste