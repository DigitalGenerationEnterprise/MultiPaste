#include "MultipasteDbusServer.h"

#include "MultipasteCore.h"

#include <QDBusConnection>
#include <QDBusMessage>

namespace multipaste {

MultipasteDbusServer::MultipasteDbusServer(MultipasteCore *core, QObject *parent)
    : QObject(parent)
    , m_core(core)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return;

    m_registered = bus.registerService(QStringLiteral("org.multipaste.MultiPaste"));
    if (m_registered)
        m_registered = bus.registerObject(QStringLiteral("/MultiPaste"), this,
                                          QDBusConnection::ExportAllProperties | QDBusConnection::ExportAllSlots |
                                              QDBusConnection::ExportAllSignals);

    if (m_core) {
        connect(m_core, &MultipasteCore::itemsChanged, this, &MultipasteDbusServer::ItemsChanged);
        connect(m_core, &MultipasteCore::positionChanged, this, &MultipasteDbusServer::PositionChanged);
        connect(m_core, &MultipasteCore::enabledChanged, this, &MultipasteDbusServer::EnabledChanged);
        connect(m_core, &MultipasteCore::logEntry, this, &MultipasteDbusServer::LogEntry);
    }
}

QStringList MultipasteDbusServer::items() const
{
    return m_core ? m_core->descriptions() : QStringList();
}

int MultipasteDbusServer::nextIndex() const
{
    return m_core ? m_core->nextIndex() : -1;
}

bool MultipasteDbusServer::enabled() const
{
    return m_core && m_core->enabled();
}

QStringList MultipasteDbusServer::activity() const
{
    return m_core ? m_core->activityLog() : QStringList();
}

QString MultipasteDbusServer::statusLine() const
{
    return m_core ? m_core->statusLine() : QStringLiteral("no core");
}

void MultipasteDbusServer::PasteNext()
{
    if (m_core)
        m_core->pasteNext();
}

void MultipasteDbusServer::NewSequence()
{
    if (m_core)
        m_core->newSequence();
}

void MultipasteDbusServer::ClearSequence()
{
    if (m_core)
        m_core->clearSequence();
}

void MultipasteDbusServer::SetEnabled(bool enabled)
{
    if (m_core)
        m_core->setEnabled(enabled);
}

void MultipasteDbusServer::Refresh()
{
    if (m_core)
        m_core->refreshFromClipboard();
}

} // namespace multipaste