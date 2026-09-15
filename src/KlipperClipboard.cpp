#include "KlipperClipboard.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDebug>

namespace multipaste {

namespace {

// QtDBus defaults to a 25 s timeout for synchronous calls. If Klipper is ever
// wedged that would freeze the tray UI for a quarter of a minute; bound every
// call instead and let the watchdog/next poll pick up the slack.
constexpr int kDbusTimeoutMs = 2000;

QDBusMessage callKlipper(const char *method, const QVariant &arg = {})
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.klipper"),
        QStringLiteral("/klipper"),
        QStringLiteral("org.kde.klipper.klipper"),
        QString::fromLatin1(method));
    if (arg.isValid())
        msg.setArguments({arg});
    return QDBusConnection::sessionBus().call(msg, QDBus::Block, kDbusTimeoutMs);
}

} // namespace

bool KlipperClipboard::available()
{
    return QDBusConnection::sessionBus().interface()->isServiceRegistered(
        QStringLiteral("org.kde.klipper"));
}

KlipperClipboard::KlipperClipboard(QObject *parent)
    : ExternalClipboard(parent)
{
    setBackendName(QStringLiteral("klipper"));

    if (!available()) {
        qWarning("MultiPaste: Klipper D-Bus service not available");
        return;
    }

    // Connect to the push signal so we don't have to poll at all.
    QDBusConnection::sessionBus().connect(
        QStringLiteral("org.kde.klipper"),
        QStringLiteral("/klipper"),
        QStringLiteral("org.kde.klipper.klipper"),
        QStringLiteral("clipboardHistoryUpdated"),
        this, SLOT(onHistoryUpdated()));

    // Emit an initial snapshot so the core sees whatever is already there.
    QMetaObject::invokeMethod(this, [this] { readNow(); }, Qt::QueuedConnection);
}

KlipperClipboard::~KlipperClipboard() = default;

void KlipperClipboard::onHistoryUpdated()
{
    readNow();
}

void KlipperClipboard::readNow()
{
    // Klipper's getClipboardContents returns the current selection as plain
    // text. Synchronous bus call; always local and bounded to 2 s.
    const QDBusMessage reply = callKlipper("getClipboardContents");
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        qWarning("MultiPaste: Klipper getClipboardContents call failed (%s)",
                 qUtf8Printable(reply.errorMessage()));
        return;
    }
    const QByteArray text = reply.arguments().constFirst().toString().toUtf8();

    if (text == m_lastRead)
        return;
    if (text == m_lastWritten)
        return; // ignore our own recent write
    m_lastRead = text;
    emit textRead(text);
}

void KlipperClipboard::requestText()
{
    readNow();
}

bool KlipperClipboard::setText(const QByteArray &text)
{
    // Record the value we are about to set so readNow() ignores it.
    m_lastWritten = text;
    m_lastRead = text; // so next read equals lastSeen, not re-captured
    const QDBusMessage reply = callKlipper("setClipboardContents",
                                           QString::fromUtf8(text));
    if (reply.type() != QDBusMessage::ErrorMessage)
        return true;

    qWarning("MultiPaste: Klipper setClipboardContents failed: %s",
             qUtf8Printable(reply.errorMessage()));
    return false;
}

} // namespace multipaste