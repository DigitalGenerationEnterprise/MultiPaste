#pragma once

#include <QObject>
#include <QString>
#include <atomic>
#include <thread>

namespace multipaste {

// Registers a KDE global shortcut (KGlobalAccel). This is the only reliable
// global trigger on a Plasma Wayland session, because Wayland does not let a
// regular client observe pointer/keyboard input of other clients. The
// shortcut is user-configurable via System Settings -> Shortcuts under the
// "multipaste" component.
class GlobalShortcutTrigger : public QObject
{
    Q_OBJECT

public:
    explicit GlobalShortcutTrigger(const QString &defaultSequence, QObject *parent = nullptr);
    ~GlobalShortcutTrigger() override;

    bool registered() const
    {
        return m_registered;
    }

    // Re-attempt registration (kglobalacceld may have been busy at startup).
    // Idempotent; returns true once registered.
    bool refresh();

signals:
    void triggered();

private:
    class Impl;
    Impl *m_impl = nullptr;
    bool m_registered = false;
};

} // namespace multipaste