#include "WaylandClipboard.h"

#include <QGuiApplication>
#include <QProcess>
#include <QStandardPaths>

namespace multipaste {

void WaylandClipboard::cleanupWriters()
{
    for (int i = m_writers.size() - 1; i >= 0; --i) {
        QProcess *writer = m_writers.at(i);
        if (writer->state() == QProcess::NotRunning) {
            m_writers.removeAt(i);
            writer->deleteLater();
        }
    }
}

bool WaylandClipboard::available()
{
    if (QGuiApplication::platformName() != QLatin1String("wayland"))
        return false;
    const auto has = [](const char *name) {
        return !QStandardPaths::findExecutable(QString::fromLatin1(name)).isEmpty();
    };
    return has("wl-paste") && has("wl-copy") && has("timeout");
}

WaylandClipboard::WaylandClipboard(QObject *parent)
    : ExternalClipboard(parent)
{
}

WaylandClipboard::~WaylandClipboard()
{
    if (m_read) {
        m_read->kill();
        m_read->deleteLater();
    }
    while (!m_writers.isEmpty()) {
        QProcess *writer = m_writers.takeFirst();
        writer->kill();
        writer->deleteLater();
    }
}

void WaylandClipboard::requestText()
{
    if (m_readActive)
        return;
    m_readActive = true;

    if (!m_read) {
        m_read = new QProcess(this);
        connect(m_read, &QProcess::finished, this, &WaylandClipboard::onReadFinished);
    }
    m_read->start(QStringLiteral("timeout"),
                  {QStringLiteral("0.6"), QStringLiteral("wl-paste"), QStringLiteral("--no-newline")});
}

void WaylandClipboard::onReadFinished()
{
    m_readActive = false;

    // `timeout` exits 124 when there is no selection; wl-paste keeps running
    // (and prints nothing) until timeout kills it. Empty means "nothing on
    // the clipboard" - still reported so callers can reset their in-flight
    // state, they simply ignore it.
    emit textRead(m_read->readAllStandardOutput());
}

bool WaylandClipboard::setText(const QByteArray &text)
{
    cleanupWriters();

    QProcess *writer = new QProcess(this);
    connect(writer, &QProcess::finished, this, [this, writer] {
        m_writers.removeAll(writer);
        writer->deleteLater();
    });
    writer->start(QStringLiteral("wl-copy"), {});
    if (!writer->waitForStarted(2000)) {
        writer->deleteLater();
        return false;
    }

    writer->write(text);
    writer->closeWriteChannel();
    m_writers.append(writer);
    return true;
}

} // namespace multipaste