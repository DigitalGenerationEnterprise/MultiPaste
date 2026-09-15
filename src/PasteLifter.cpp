#include "PasteLifter.h"

#include <QGuiApplication>
#include <QProcess>
#include <QStandardPaths>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <X11/Xutil.h>

namespace multipaste {

PasteLifter::PasteLifter(QObject *parent)
    : QObject(parent)
{
}

PasteLifter::~PasteLifter()
{
    closeDisplay();
}

void PasteLifter::closeDisplay()
{
    if (m_display) {
        XCloseDisplay(m_display);
        m_display = nullptr;
    }
}

PasteLifter::Kind PasteLifter::kind()
{
    const QString platform = QGuiApplication::platformName();
    if (platform == QLatin1String("xcb"))
        return Kind::X11;
    if (platform == QLatin1String("wayland"))
        return Kind::Wayland;
    return Kind::Unknown;
}

bool PasteLifter::isAutomatic() const
{
    switch (kind()) {
    case Kind::X11:
        return true;
    case Kind::Wayland:
        return !QStandardPaths::findExecutable(QStringLiteral("ydotool")).isEmpty()
            || !QStandardPaths::findExecutable(QStringLiteral("wtype")).isEmpty();
    case Kind::Unknown:
        return false;
    }
    return false;
}

bool PasteLifter::paste()
{
    switch (kind()) {
    case Kind::X11:
        return pasteX11();
    case Kind::Wayland:
        return pasteWayland();
    case Kind::Unknown:
        return false;
    }
    return false;
}

bool PasteLifter::pasteX11()
{
    if (!m_display)
        m_display = XOpenDisplay(nullptr);
    if (!m_display)
        return false;

    const KeyCode ctrl = XKeysymToKeycode(m_display, XK_Control_L);
    const KeyCode v = XKeysymToKeycode(m_display, XK_v);
    if (ctrl == 0 || v == 0) {
        qWarning("PasteLifter: could not resolve keycodes for Ctrl+V");
        return false;
    }

    XTestFakeKeyEvent(m_display, ctrl, True, CurrentTime);
    XTestFakeKeyEvent(m_display, v, True, CurrentTime);
    XTestFakeKeyEvent(m_display, v, False, CurrentTime);
    XTestFakeKeyEvent(m_display, ctrl, False, CurrentTime);
    XFlush(m_display);
    return true;
}

bool PasteLifter::waylandBackoffActive() const
{
    // After three consecutive failures wait 60 s before probing the helpers
    // again - a compositor without the virtual-keyboard protocol will keep
    // failing forever, and spawning a process for every paste is pointless.
    return m_consecutiveWaylandFails >= 3 && m_lastWaylandFail.isValid()
        && m_lastWaylandFail.elapsed() < 60000;
}

bool PasteLifter::pasteWayland()
{
    if (waylandBackoffActive()) {
        qInfo("PasteLifter: skipping Wayland injection (helper failed recently, backoff)");
        return false;
    }

    // Linux input keycodes: 29 = KEY_LEFTCTRL, 47 = KEY_V
    static const QString ydotool = QStringLiteral("ydotool");
    static const QStringList ydotoolArgs = {QStringLiteral("key"), QStringLiteral("29:1"), QStringLiteral("47:1"), QStringLiteral("47:0"), QStringLiteral("29:0")};

    static const QString wtype = QStringLiteral("wtype");
    static const QStringList wtypeArgs = {QStringLiteral("-M"), QStringLiteral("ctrl"), QStringLiteral("-P"), QStringLiteral("v"), QStringLiteral("-p"), QStringLiteral("v"), QStringLiteral("-m"), QStringLiteral("ctrl")};

    QStringList candidates;
    if (!QStandardPaths::findExecutable(ydotool).isEmpty())
        candidates.append(ydotool);
    if (!QStandardPaths::findExecutable(wtype).isEmpty())
        candidates.append(wtype);

    for (const QString &tool : std::as_const(candidates)) {
        const QStringList args = (tool == ydotool) ? ydotoolArgs : wtypeArgs;
        const int exitCode = QProcess::execute(tool, args);
        if (exitCode == 0) {
            m_consecutiveWaylandFails = 0;
            return true;
        }
        qWarning("PasteLifter: '%s' failed with exit code %d", qUtf8Printable(tool), exitCode);
        ++m_consecutiveWaylandFails;
        m_lastWaylandFail.start();
    }
    return false;
}

} // namespace multipaste