#include "GlobalShortcutTrigger.h"

#if defined(MULTIPASTE_HAS_KGLOBALACCEL)
#include <kglobalaccel.h>
#include <QAction>
#include <QKeySequence>
#endif

class QAction;

namespace multipaste {

class GlobalShortcutTrigger::Impl : public QObject
{
    Q_OBJECT

public:
    Impl(const QString &defaultSequences, QObject *parent)
        : QObject(parent)
        , m_defaultSequences(defaultSequences)
    {
#if defined(MULTIPASTE_HAS_KGLOBALACCEL)
        // objectName is persisted in kglobalshortcutsrc and MUST stay stable.
        QAction *action = new QAction(QStringLiteral("Paste next sequence item"), this);
        m_action = action;
        action->setObjectName(QStringLiteral("paste_next"));
        action->setText(QStringLiteral("Paste next sequence item"));

        m_registered = registerSequences();
#endif
    }

    bool registered() const
    {
        return m_registered;
    }

    bool refresh()
    {
#if defined(MULTIPASTE_HAS_KGLOBALACCEL)
        if (m_registered || !m_action)
            return true;
        m_registered = registerSequences();
        return m_registered;
#else
        // Built without KDE Frameworks: no global shortcut available.
        return false;
#endif
    }

signals:
    void fired();

private:
    // Registers every comma-separated sequence from the setting, e.g.
    // "Alt+Shift+V,Meta+Shift+V" so any of them triggers a paste.
    bool registerSequences()
    {
#if defined(MULTIPASTE_HAS_KGLOBALACCEL)
        if (!m_action)
            return false;

        const QStringList parts = m_defaultSequences.split(QLatin1Char(','),
                                                          Qt::SkipEmptyParts);
        QList<QKeySequence> sequences;
        for (const QString &part : parts) {
            const QKeySequence seq = QKeySequence::fromString(part.trimmed());
            if (!seq.isEmpty())
                sequences.append(seq);
        }
        if (sequences.isEmpty())
            return false;

        if (KGlobalAccel::self()->setShortcut(m_action, sequences)) {
            KGlobalAccel::self()->setDefaultShortcut(m_action, sequences);
            if (!m_connected) {
                connect(m_action, &QAction::triggered, this, &Impl::fired);
                m_connected = true;
            }
            return true;
        }
        return false;
#else
        // Built without KDE Frameworks: no global shortcut available.
        return false;
#endif
    }

    QAction *m_action = nullptr;
    bool m_registered = false;
    bool m_connected = false;
    QString m_defaultSequences;
};

GlobalShortcutTrigger::GlobalShortcutTrigger(const QString &defaultSequence, QObject *parent)
    : QObject(parent)
{
    m_impl = new Impl(defaultSequence, this);
    m_registered = m_impl->registered();
    if (m_impl)
        connect(m_impl, &Impl::fired, this, &GlobalShortcutTrigger::triggered);
}

bool GlobalShortcutTrigger::refresh()
{
    bool ok = m_impl ? m_impl->refresh() : false;
    if (ok)
        m_registered = true;
    return ok;
}

GlobalShortcutTrigger::~GlobalShortcutTrigger() = default;

} // namespace multipaste

#include "GlobalShortcutTrigger.moc"