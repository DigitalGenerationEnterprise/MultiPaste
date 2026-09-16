#include "MultipasteCore.h"

#include "PasteLifter.h"
#include "KlipperClipboard.h"
#include "WaylandClipboard.h"

#include <QGuiApplication>
#include <QTimer>
#include <QDebug>
#include <QCryptographicHash>
#include <algorithm>

namespace multipaste {

namespace {
constexpr int kSelfMarkRingSize = 8;
constexpr int kSelfMarkLifetimeMs = 3000;

QString formatsSignature(const QMimeData *md)
{
    if (!md || md->formats().isEmpty())
        return QString();
    QStringList f = md->formats();
    f.sort();
    return f.join(u',');
}

// SHA-256 of the payload's text form (empty if the payload carries no text).
// Used for the restart-guard seed: what Klipper hands back after a write/read
// round-trip is (re)constructed as a plain text/plain payload, so comparing on
// the text bytes - not the MIME-aware fingerprintOf() - is the stable signal.
QByteArray textSeedHash(const QMimeData *md)
{
    if (!md)
        return QByteArray();
    const QString text = md->text();
    if (text.isEmpty())
        return QByteArray();
    return QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256).toHex();
}

ClipboardBackend autoPickBackend()
{
    // Klipper is only meaningful attached to a real desktop selection (its
    // clipboardHistoryUpdated/getClipboardContents are synced to the running
    // session). On test platforms (offscreen/minimal) always use QClipboard.
    const QString p = QGuiApplication::platformName();
    if (p != QLatin1String("offscreen") && p != QLatin1String("minimal")
        && KlipperClipboard::available())
        return ClipboardBackend::Klipper; // native Plasma clipboard daemon
    if (WaylandClipboard::available())
        return ClipboardBackend::Wayland;
    return ClipboardBackend::Qt;
}
} // namespace

MultipasteCore::MultipasteCore(QObject *parent)
    : QObject(parent)
{
    m_clipboard = QGuiApplication::clipboard();
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(m_pollIntervalMs);
    connect(m_pollTimer, &QTimer::timeout, this, &MultipasteCore::onPollTick);
}

void MultipasteCore::createExternalBackend()
{
    ClipboardBackend b = m_backend;
    if (b == ClipboardBackend::Auto)
        b = autoPickBackend();

    switch (b) {
    case ClipboardBackend::Klipper: {
        m_external = new KlipperClipboard(this);
        break;
    }
    case ClipboardBackend::Wayland: {
        m_external = new WaylandClipboard(this);
        break;
    }
    case ClipboardBackend::Auto:
    case ClipboardBackend::Qt:
        break; // plain QClipboard
    }
    if (m_external)
        connect(m_external, &ExternalClipboard::textRead, this, &MultipasteCore::onExternalText);
}

MultipasteCore::~MultipasteCore()
{
    stop();
    delete m_lastForeignCopy;
}

void MultipasteCore::setBackend(ClipboardBackend b)
{
    // Allow a live switch after start() as well: resolves the autostart race
    // where the app comes up before Plasma's clipboard daemon is registered.
    if (m_backend == b || (b == ClipboardBackend::Auto && m_external))
        return;
    m_backend = b;

    if (m_external) {
        // The old backend may still hand callbacks in (in-flight wl-paste);
        // detach it so the core only talks to the replacement.
        m_external->disconnect(this);
        m_external->deleteLater();
        m_external = nullptr;
        m_externalReadPending = false;
    }
    if (m_running) {
        createExternalBackend();
        requestExternalText();
    }
}

void MultipasteCore::start()
{
    if (m_running)
        return;
    m_running = true;
    if (!m_external)
        createExternalBackend();
    connect(m_clipboard, &QClipboard::dataChanged, this, &MultipasteCore::onClipboardChanged,
            Qt::UniqueConnection);
    m_pollTimer->start();
    if (m_external)
        requestExternalText();
}

void MultipasteCore::stop()
{
    if (!m_running)
        return;
    m_running = false;
    disconnect(m_clipboard, &QClipboard::dataChanged, this, &MultipasteCore::onClipboardChanged);
    m_pollTimer->stop();
    m_emptyRetryPending = false;
}

void MultipasteCore::setEnabled(bool b)
{
    if (m_enabled == b)
        return;
    m_enabled = b;
    appendLog(b ? QStringLiteral("monitoring enabled") : QStringLiteral("monitoring paused"));
    emit enabledChanged(b);
    emit itemsChanged();
}

void MultipasteCore::setMaxEntries(int n)
{
    m_maxEntries = qMax(1, n);
    prune();
}

void MultipasteCore::setPollIntervalMs(int ms)
{
    m_pollIntervalMs = qMax(100, ms);
    if (m_pollTimer)
        m_pollTimer->setInterval(m_pollIntervalMs);
}

void MultipasteCore::setRestoreAfterPaste(bool b)
{
    m_restoreAfterPaste = b;
}

void MultipasteCore::setRestoreDelayMs(int ms)
{
    m_restoreDelayMs = qMax(0, ms);
}

int MultipasteCore::nextIndex() const
{
    if (m_items.empty())
        return -1;
    const int n = int(m_items.size());
    if (m_next >= n)
        return m_next % n;
    return m_next;
}

HistoryItem *MultipasteCore::itemAt(int i)
{
    return (i >= 0 && size_t(i) < m_items.size()) ? &m_items[i] : nullptr;
}

const HistoryItem *MultipasteCore::itemAt(int i) const
{
    return (i >= 0 && size_t(i) < m_items.size()) ? &m_items[i] : nullptr;
}

QString MultipasteCore::statusLine() const
{
    if (!m_enabled)
        return QStringLiteral("disabled");
    return QStringLiteral("%1 items").arg(int(m_items.size()));
}

QString MultipasteCore::positionLine() const
{
    if (m_items.empty())
        return QStringLiteral("no items");
    const int idx = nextIndex();
    return QStringLiteral("%1 / %2").arg(idx + 1).arg(int(m_items.size()));
}

static bool captureDebug()
{
    return qEnvironmentVariableIsSet("MULTIPASTE_DEBUG");
}

// ---------------------------------------------------------------------------
// Capture path
// ---------------------------------------------------------------------------

void MultipasteCore::onClipboardChanged()
{
    if (!m_running || !m_enabled)
        return;
    if (m_insideSelfWrite)
        return; // synchronous dataChanged raised by our own setMimeData
    tryCapture();
}

void MultipasteCore::onPollTick()
{
    if (!m_running || !m_enabled)
        return;
    if (m_external) {
        // Cheap refresh: one read against the selected backend (Klipper D-Bus
        // getClipboardContents, or the wl-paste bridge). The Klipper path is
        // additionally push-driven via its clipboardHistoryUpdated signal.
        requestExternalText();
        return;
    }
    const QString sig = formatsSignature(m_clipboard->mimeData());

    // Wayland quirk: a dataChanged event can arrive before the offer is
    // readable, and the empty signature can then permanently match the "last
    // foreign" one - so never treat an empty offer as a reason to stop trying.
    // The retry is cheap and backoff is enforced inside tryCapture().
    if (sig.isEmpty()) {
        tryCapture();
        return;
    }
    if (sig == m_lastForeignFormatsSignature)
        return;
    tryCapture();
}

void MultipasteCore::tryCapture()
{
    if (m_external) {
        requestExternalText();
        return;
    }
    const QMimeData *md = m_clipboard->mimeData();
    if (!md || md->formats().isEmpty()) {
        // Wayland transfers arrive asynchronously: a change may be signalled
        // before the offer is readable, so retry once shortly afterwards.
        if (!m_emptyRetryPending && !m_clipboard->ownsClipboard()) {
            m_emptyRetryPending = true;
            QTimer::singleShot(150, this, [this] {
                m_emptyRetryPending = false;
                if (m_running && m_enabled)
                    tryCapture();
            });
        }
        return;
    }
    acceptPayload(md);
}

void MultipasteCore::requestExternalText()
{
    if (!m_running || !m_enabled)
        return;
    if (m_externalReadPending)
        return;
    m_externalReadPending = true;
    m_external->requestText();
}

void MultipasteCore::onExternalText(const QByteArray &text)
{
    m_externalReadPending = false;
    if (!m_running || !m_enabled)
        return;
    if (text.isEmpty())
        return; // clipboard emptied or not readable - keep polling
    if (text == m_lastExternalText)
        return; // unchanged since last accepted read - avoid log spam

    m_lastExternalText = text;
    QMimeData md;
    md.setText(QString::fromUtf8(text));
    acceptPayload(&md);
}

QByteArray MultipasteCore::currentClipboardText() const
{
    return m_lastExternalText;
}

QByteArray MultipasteCore::lastExternalTextFingerprintHex() const
{
    if (m_lastExternalText.isEmpty())
        return QByteArray();
    return QCryptographicHash::hash(m_lastExternalText, QCryptographicHash::Sha256).toHex();
}

void MultipasteCore::acceptPayload(const QMimeData *md)
{
    const QByteArray fp = fingerprintOf(md);
    if (fp.isEmpty())
        return;

// Ignore our own temporary writes (async dataChanged case).
    if (isSelfContent(fp))
        return;

    // Launch-time suppression: keep dropping payloads whose TEXT equals what
    // the previous run left on the clipboard (the seed). The seed stays armed
    // until the clipboard actually changes - a repeated identical read (Klipper
    // re-emit, poll safety-net) must not turn into a fresh history entry. The
    // first different payload clears the guard and is captured normally.
    if (!m_seedFingerprint.isEmpty()) {
        const QByteArray seedHash = textSeedHash(md);
        if (!seedHash.isEmpty() && seedHash == m_seedFingerprint) {
            appendLog(QStringLiteral("ignored pre-existing clipboard (previous run)"));
            return;
        }
        m_seedFingerprint.clear();
    }

    // Skip empty payloads (e.g. a clipboard clear).
    qint64 bytes = 0;
    fingerprintOf(md, &bytes);
    if (bytes == 0) {
        appendLog(QStringLiteral("COPIED empty/ignored"));
        return;
    }

    // Skip consecutive duplicates by fingerprint (exact MIME set + bytes).
    if (!m_items.empty() && m_items.back().fingerprint() == fp) {
        appendLog(QStringLiteral("COPIED again (duplicate)"));
        return;
    }

    // Same visible plain text but different MIME sets (e.g. an X11 race between
    // clipboard owners, or copy from two apps that offer different formats):
    // the user would paste the same thing twice, so drop it as well.
    const QByteArray text = md->data(QStringLiteral("text/plain"));
    const QStringList formats = md->formats();
    if (formats.contains(QStringLiteral("text/plain")) && !text.isEmpty()
        && !m_items.empty()) {
        const QByteArray next = m_items.back().mime()->data(QStringLiteral("text/plain"));
        if (!next.isEmpty() && next == text) {
            appendLog(QStringLiteral("COPIED again (same text)"));
            return;
        }
    }

    HistoryItem item(HistoryItem::deepCopy(md));
    if (item.isEmpty())
        return;

    m_items.push_back(std::move(item));

    // Remember the most recent foreign copy so we can restore it after paste.
    delete m_lastForeignCopy;
    m_lastForeignCopy = HistoryItem::deepCopy(md).release();
    m_lastForeignFormatsSignature = formatsSignature(md);

    // Track the last accepted external text so the restart-guard seed is
    // meaningful on every capture path (onExternalText already does this for
    // the external backends; QClipboard never goes through it).
    if (!text.isEmpty())
        m_lastExternalText = text;

    appendLog(QStringLiteral("COPIED  %1  (item %2 of %3)")
                  .arg(m_items.back().preview())
                  .arg(m_items.size())
                  .arg(m_items.size()));

    if (captureDebug())
        qInfo("MULTIPASTE_DEBUG capture #%d: %s", int(m_items.size()),
              qUtf8Printable(m_items.back().description()));

    prune();
    emit itemsChanged();
    emit positionChanged();
}

void MultipasteCore::prune()
{
    bool removedAny = false;
    while (m_items.size() > size_t(m_maxEntries)) {
        m_items.erase(m_items.begin());
        removedAny = true;
        if (m_next > 0)
            --m_next;
    }
    if (m_next >= int(m_items.size()) && !m_items.empty())
        m_next = m_next % int(m_items.size());
    if (removedAny) {
        emit itemsChanged();
        emit positionChanged();
    }
}

// ---------------------------------------------------------------------------
// Self-write suppression
// ---------------------------------------------------------------------------

void MultipasteCore::markSelfWrite(QMimeData *md)
{
    const QByteArray fp = fingerprintOf(md);
    if (fp.isEmpty())
        return;
    SelfMark mark;
    mark.fingerprint = fp;
    mark.at.start();
    m_recentSelfMarks.append(mark);
    while (m_recentSelfMarks.size() > kSelfMarkRingSize)
        m_recentSelfMarks.removeFirst();
}

bool MultipasteCore::isSelfContent(const QByteArray &fingerprint) const
{
    for (auto it = m_recentSelfMarks.crbegin(); it != m_recentSelfMarks.crend(); ++it) {
        if (it->fingerprint == fingerprint && it->at.elapsed() < kSelfMarkLifetimeMs)
            return true;
    }
    return false;
}

void MultipasteCore::setClipboardIgnoring(QMimeData *md)
{
    markSelfWrite(md);
    if (m_external) {
        // Klipper (native) or the wl-copy bridge: either can set the
        // selection while we have no focus, which QClipboard cannot.
        const QByteArray text = md->data(QStringLiteral("text/plain"));
        if (!text.isEmpty()) {
            // Skip redundant writes: no new D-Bus call / wl-copy client when
            // the value we are about to set is already the one we read/wrote.
            if (text != m_lastExternalText)
                m_external->setText(text);
            m_lastExternalText = text; // so our own writes are not re-read as a new copy
            return;
        }
    }
    m_insideSelfWrite = true;
    m_clipboard->setMimeData(md);
    m_insideSelfWrite = false;
}

QString MultipasteCore::backendName() const
{
    if (m_external)
        return m_external->backendName();
    return QStringLiteral("qt");
}

// ---------------------------------------------------------------------------
// Sequence / paste
// ---------------------------------------------------------------------------

void MultipasteCore::pasteNext()
{
    if (!m_running || !m_enabled) {
        appendLog(QStringLiteral("PASTE ignored (disabled)"));
        emit errorReported(QStringLiteral("MultiPaste is disabled"));
        return;
    }
    if (m_items.empty()) {
        appendLog(QStringLiteral("PASTE ignored (history empty)"));
        emit errorReported(QStringLiteral("clipboard history is empty"));
        return;
    }

    const int idx = nextIndex();
    HistoryItem &item = m_items[idx];
    const QString description = item.preview();
    const QByteArray fingerprint = item.fingerprint();

    setClipboardIgnoring(HistoryItem::deepCopy(item.mime()).release());
    m_next = (m_next + 1) % int(m_items.size());
    emit positionChanged();

    if (m_lifter && m_lifter->paste()) {
        appendLog(QStringLiteral("PASTED  %1  -> injected, next is %2 of %3")
                      .arg(description)
                      .arg(m_next + 1)
                      .arg(m_items.size()));
        if (captureDebug())
            qInfo("MULTIPASTE_DEBUG injected paste, next index -> %d",
                  int(m_next % int(m_items.size())));
        emit pastePerformed(description);
        if (m_restoreAfterPaste) {
            QTimer::singleShot(std::max(0, m_restoreDelayMs), this, [this, fingerprint, description] {
                doRestoreAfterPaste(fingerprint, description);
            });
        }
    } else {
        // Cannot inject automatically (Wayland without helper): leave the item
        // on the clipboard and let the user paste it with a normal Ctrl+V.
        appendLog(QStringLiteral("PASTED  %1  -> loaded, press Ctrl+V").arg(description));
        emit manualPasteHint(description);
    }
}

void MultipasteCore::doRestoreAfterPaste(const QByteArray &pastedFingerprint,
                                         const QString &description)
{
    if (!m_running)
        return;

    const QMimeData *current = m_clipboard->mimeData();
    QByteArray currentFp;
    if (m_external) {
        const QByteArray t = currentClipboardText();
        if (!t.isEmpty()) {
            QMimeData md;
            md.setText(QString::fromUtf8(t));
            currentFp = fingerprintOf(&md);
        }
    } else {
        currentFp = fingerprintOf(current);
    }

    // If the user copied something else during the paste window, do not
    // clobber it with our restore.
    if (!currentFp.isEmpty() && currentFp != pastedFingerprint) {
        appendLog(QStringLiteral("restore skipped: clipboard changed during paste"));
        if (captureDebug())
            qInfo("MULTIPASTE_DEBUG restore skipped: clipboard changed during paste window");
        emit errorReported(QStringLiteral("clipboard changed during paste; skipped restore"));
        return;
    }

    if (!m_lastForeignCopy) {
        emit errorReported(QStringLiteral("nothing to restore"));
        return;
    }

    const QByteArray restoreFp = fingerprintOf(m_lastForeignCopy);
    if (restoreFp.isEmpty() || restoreFp == pastedFingerprint)
        return; // already the same content as what we pasted

    setClipboardIgnoring(HistoryItem::deepCopy(m_lastForeignCopy).release());
    appendLog(QStringLiteral("restored previous clipboard (paste window)"));
    qInfo("MultiPaste: pasted %s, restored previous clipboard", qUtf8Printable(description));
}

void MultipasteCore::newSequence()
{
    m_next = 0;
    appendLog(QStringLiteral("NEW SEQUENCE - next paste will be item 1"));
    emit positionChanged();
}

void MultipasteCore::clearSequence()
{
    m_items.clear();
    m_next = 0;
    delete m_lastForeignCopy;
    m_lastForeignCopy = nullptr;
    m_lastForeignFormatsSignature.clear();
    m_recentSelfMarks.clear(); // re-copying old content must work again
    appendLog(QStringLiteral("SEQUENCE CLEARED"));
    emit itemsChanged();
    emit positionChanged();
}

QStringList MultipasteCore::descriptions() const
{
    QStringList out;
    out.reserve(int(m_items.size()));
    for (const HistoryItem &it : m_items)
        out.append(it.preview());
    return out;
}

QStringList MultipasteCore::activityLog() const
{
    // newest first
    QStringList out = m_activityLog;
    std::reverse(out.begin(), out.end());
    return out;
}

void MultipasteCore::setActivityLogSize(int n)
{
    m_activityLogSize = qMax(10, n);
    while (m_activityLog.size() > m_activityLogSize)
        m_activityLog.removeFirst();
}

void MultipasteCore::refreshFromClipboard()
{
    m_emptyRetryPending = false;
    if (!m_running)
        return;
    // Re-read whatever is on the clipboard now - repairs a missed Wayland
    // transfer without recording a false duplicate (fingerprint guard).
    const size_t before = m_items.size();
    tryCapture();
    if (m_items.size() > before)
        appendLog(QStringLiteral("repair: re-read clipboard"));
}

void MultipasteCore::appendLog(const QString &line)
{
    m_activityLog.append(line);
    while (m_activityLog.size() > m_activityLogSize)
        m_activityLog.removeFirst();
    emit logEntry(line);
}

} // namespace multipaste