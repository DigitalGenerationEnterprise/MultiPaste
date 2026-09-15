#include "X11MouseTrigger.h"

#include <QMetaObject>
#include <QThread>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

namespace multipaste {

X11MouseTrigger::X11MouseTrigger(QObject *parent)
    : QObject(parent)
{
    m_available = true;
    m_thread = std::thread([this] { run(); });
}

X11MouseTrigger::~X11MouseTrigger()
{
    stop();
    if (m_thread.joinable())
        m_thread.join();
}

void X11MouseTrigger::stop()
{
    m_stop = true;
}

void X11MouseTrigger::rearm()
{
    m_rearm = true;
}

void X11MouseTrigger::run()
{
    Display *dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        // No X server around (e.g. native Wayland). Trigger simply stays inert.
        m_available = false;
        return;
    }

    // Watch for Alt (Mod1) + Right button (Button3), passively, without
    // stealing the events from the focused application.
    const Window root = DefaultRootWindow(dpy);
    auto grab = [&] {
        XGrabButton(dpy, Button3, Mod1Mask, root,
                    True, // owner_events: keep normal delivery to the app
                    ButtonPressMask | ButtonReleaseMask,
                    GrabModeAsync, GrabModeAsync,
                    None, None);
        XSync(dpy, False);
    };
    grab();

    // Pump X events until we are asked to stop. Never block in XNextEvent():
    // only consume events that are already pending, then nap briefly so the
    // thread stays responsive and burns almost no CPU.
    XEvent ev;
    while (!m_stop.load(std::memory_order_relaxed)) {
        if (m_rearm.exchange(false))
            grab(); // self-repair: re-issue the passive grab
        if (XPending(dpy) == 0) {
            QThread::msleep(10);
            continue;
        }
        XNextEvent(dpy, &ev);
        if (ev.type != ButtonPress)
            continue;
        if (ev.xbutton.button != Button3)
            continue;
        if (!(ev.xbutton.state & Mod1Mask))
            continue;

        QMetaObject::invokeMethod(this, [this] { emit triggered(); }, Qt::QueuedConnection);
    }

    XUngrabButton(dpy, Button3, Mod1Mask, root);
    XCloseDisplay(dpy);
}

} // namespace multipaste