#include "KlipperClipboard.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDebug>

namespace multipaste {

bool KlipperClipboard::available()
{
    return QDBusConnection::sessionBus().interface()->isServiceRegistered(
        QStringLiteral("org.kde.klipper"));
}

KlipperClipboard::KlipperClipboard(QObject *parent)
    : ExternalClipboard(parent),
      m_iface(QStringLiteral("org.kde.klipper"),
              QStringLiteral("/klipper"),
              QStringLiteral("org.kde.klipper.klipper"),
              QDBusConnection::sessionBus(), this)
{
    setBackendName(QStringLiteral("klipper"));

    if (!m_iface.isValid()) {
        qWarning("MultiPaste: Klipper D-Bus interface not valid");
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
    // text. Synchronous bus call; always local and fast.
    const QDBusMessage reply = m_iface.call(QStringLiteral("getClipboardContents"));
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        qWarning("MultiPaste: Klipper getClipboardContents call failed");
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
    if (!m_iface.isValid())
        return false;

    // Record the value we are about to set so readNow() ignores it.
    m_lastWritten = text;
    m_lastRead = text; // so next read equals lastSeen, not re-captured
    const QDBusMessage reply = m_iface.call(QStringLiteral("setClipboardContents"),
                                            QString::fromUtf8(text));
    if (reply.type() != QDBusMessage::ErrorMessage)
        return true;

    qWarning("MultiPaste: Klipper setClipboardContents failed: %s",
             qUtf8Printable(reply.errorMessage()));
    return false;
}

} // namespace multipaste