#pragma once

#include <QObject>
#include <atomic>
#include <thread>

namespace multipaste {

// Global "Alt + Right Mouse Button" trigger for X11 sessions.
//
// A passive X grab on the root window lets us observe the combination without
// disturbing normal pointer delivery: the click still reaches the focused
// application underneath (owner_events = True, passive grab). Events are read
// on a dedicated thread so we never fight the Qt event loop.
//
// This backend does nothing on native Wayland (a Wayland client cannot see
// other clients' input); use GlobalShortcutTrigger there instead.
class X11MouseTrigger : public QObject
{
    Q_OBJECT

public:
    explicit X11MouseTrigger(QObject *parent = nullptr);
    ~X11MouseTrigger() override;

    bool isAvailable() const
    {
        return m_available;
    }

    // Self-repair: requests that the grab thread re-issue the passive grab
    // (protects against an X server reset or a lost/overwritten grab). Cheap.
    void rearm();

signals:
    void triggered();

private:
    void run();
    void stop();

    std::atomic<bool> m_available{false}; // written by run(), read by main thread
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_rearm{false}; // set by rearm(), consumed by run()
    std::thread m_thread;
};

} // namespace multipaste