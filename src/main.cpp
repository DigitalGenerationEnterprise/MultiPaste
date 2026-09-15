#include "ActivityPanel.h"
#include "GlobalShortcutTrigger.h"
#include "MultipasteCore.h"
#include "MultipasteDbusServer.h"
#include "PasteLifter.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "TrayIcon.h"
#include "X11MouseTrigger.h"

#include <QApplication>
#include <QDBusConnection>
#include <QDBusError>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>

#include <memory>

using namespace multipaste;

namespace {

QString autostartFilePath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
        .filePath(QStringLiteral("autostart/multipaste.desktop"));
}

void applyAutostart(bool enabled)
{
    const QString path = autostartFilePath();
    if (!enabled) {
        QFile::remove(path);
        return;
    }

    QDir dir = QFileInfo(path).dir();
    if (!dir.exists())
        dir.mkpath(QStringLiteral("."));

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        qWarning("MultiPaste: could not write autostart file %s", qUtf8Printable(path));
        return;
    }
    QTextStream out(&f);
    out << "[Desktop Entry]\n"
        << "Type=Application\n"
        << "Name=MultiPaste\n"
        << "Comment=Sequential multi-copy paste utility\n"
        << "Exec=" << QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath() << "\n"
        << "X-GNOME-Autostart-enabled=true\n"
        << "Terminal=false\n";
    f.close();
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("multipaste"));
    QApplication::setApplicationDisplayName(QStringLiteral("MultiPaste"));
    QApplication::setOrganizationName(QStringLiteral("multipaste"));
    QApplication::setQuitOnLastWindowClosed(false);

    // Single instance via a lock file in the user's temp dir. A crashed
    // previous instance leaves the lock behind; QLockFile considers it stale
    // after this many ms and removes it (self-repair).
    QLockFile lock(QDir::temp().filePath(QStringLiteral("multipaste.lock")));
    lock.setStaleLockTime(5000);
    if (!lock.tryLock(100)) {
        qWarning("MultiPaste is already running.");
        return 0;
    }

    Settings settings;

    // Backend selection: --backend=auto|klipper|wayland|qt (also
    // MULTIPASTE_BACKEND). Default "auto" prefers Klipper, Plasma's native
    // clipboard daemon - no polling processes, no extra data-control clients.
    ClipboardBackend backend = ClipboardBackend::Auto;
    const QStringList args = QCoreApplication::arguments();
    QString backendName;
    for (int i = 1; i < args.size(); ++i) {
        const QString a = args.at(i);
        if (a == QLatin1String("--backend") && i + 1 < args.size())
            backendName = args.at(++i);
        else if (a.startsWith(QLatin1String("--backend=")))
            backendName = a.mid(10);
    }
    if (backendName.isEmpty())
        backendName = qEnvironmentVariable("MULTIPASTE_BACKEND");
    if (backendName == QLatin1String("klipper"))
        backend = ClipboardBackend::Klipper;
    else if (backendName == QLatin1String("wayland"))
        backend = ClipboardBackend::Wayland;
    else if (backendName == QLatin1String("qt"))
        backend = ClipboardBackend::Qt;
    else if (!backendName.isEmpty())
        qWarning("MultiPaste: unknown backend '%s' (use klipper|wayland|qt|auto)",
                 qUtf8Printable(backendName));

    const QString platform = QGuiApplication::platformName();

    PasteLifter lifter;
    MultipasteCore core;
    core.setBackend(backend);
    core.setPasteLifter(&lifter);
    core.setEnabled(settings.enabled());
    core.setMaxEntries(settings.maxEntries());
    core.setPollIntervalMs(settings.pollIntervalMs());
    core.setRestoreAfterPaste(settings.restoreAfterPaste());
    core.setRestoreDelayMs(settings.restoreDelayMs());
    core.setActivityLogSize(settings.activityLogSize());

    TrayIcon tray(&core, &settings);

    // Session-bus service used by the Plasma 6 widget and external tools.
    // Registered before the triggers below because shortcut registration can
    // take seconds when kglobalaccel is slow; the widget must not wait for it.
    MultipasteDbusServer dbus(&core);
    if (!dbus.isRegistered())
        qWarning("MultiPaste: DBus service org.multipaste.MultiPaste not available: %s",
                 qUtf8Printable(QDBusConnection::sessionBus().lastError().message()));

    // JSON snapshot file the Plasma 6 widget reads (no extra QML DBus module
    // needed). Written on every relevant change.
    const QString snapDir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
                            + QStringLiteral("/multipaste");
    const QString snapPath = snapDir + QStringLiteral("/activity.json");
    auto writeSnapshot = [snapPath, &core] {
        QDir().mkpath(QFileInfo(snapPath).absolutePath());
        QJsonArray items;
        const QStringList descs = core.descriptions();
        for (const QString &d : descs)
            items.append(QJsonValue(d));
        const int next = core.positionLine().section(QLatin1Char('/'), 0, 0).toInt() - 1;
        QJsonArray activity;
        const QStringList log = core.activityLog();
        for (const QString &l : log)
            activity.append(QJsonValue(l));
        const QJsonObject obj{{QStringLiteral("status"), QStringLiteral("ok")},
                              {QStringLiteral("enabled"), core.enabled()},
                              {QStringLiteral("next"), next},
                              {QStringLiteral("items"), items},
                              {QStringLiteral("activity"), activity}};
        QFile f(snapPath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
            f.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    };
    QObject::connect(&core, &MultipasteCore::itemsChanged, &app, writeSnapshot);
    QObject::connect(&core, &MultipasteCore::positionChanged, &app, writeSnapshot);
    QObject::connect(&core, &MultipasteCore::enabledChanged, &app, writeSnapshot);
    QObject::connect(&core, &MultipasteCore::logEntry, &app, writeSnapshot);

    // Trigger 1 (all sessions): KDE global shortcut. This is the only fully
    // global trigger on Wayland.
    const auto gst = std::make_unique<GlobalShortcutTrigger>(settings.shortcut());
    QObject::connect(gst.get(), &GlobalShortcutTrigger::triggered, &core, [&core] {
        core.pasteNext();
    });
    if (!gst->registered())
        qWarning("MultiPaste: could not register the global shortcut '%s'.",
                 qUtf8Printable(settings.shortcut()));

    // Trigger 2 (X11 sessions only): Alt + Right mouse button.
    std::unique_ptr<X11MouseTrigger> mouse;
    if (platform == QLatin1String("xcb") && settings.x11MouseTrigger()) {
        mouse = std::make_unique<X11MouseTrigger>();
        QObject::connect(mouse.get(), &X11MouseTrigger::triggered, &core, [&core] {
            core.pasteNext();
        });
        if (!mouse->isAvailable())
            qWarning("MultiPaste: X11 mouse trigger unavailable. Watch for the tray warning.");
    }

    // Fail-over / self-repair: watch the triggers and the clipboard, and fix
    // the common degradations without a restart. Automatic runs are silent
    // unless something actually got repaired; the manual tray action always
    // reports.
    auto runRepair = [&](bool notify) {
        const int itemsBefore = core.count();
        core.refreshFromClipboard(); // re-read a missed Wayland transfer
        const bool clipboardFixed = core.count() != itemsBefore;
        bool shortcutFixed = false;
        if (gst && !gst->registered() && gst->refresh())
            shortcutFixed = true;
        if (mouse)
            mouse->rearm(); // re-issue the X11 passive grab after a server reset
        if (notify || clipboardFixed || shortcutFixed)
            tray.showMessage(QStringLiteral("MultiPaste"),
                             QStringLiteral("Repair finished."));
    };
    QTimer repairWatchdog;
    repairWatchdog.setInterval(30000);
    repairWatchdog.start();
    QObject::connect(&repairWatchdog, &QTimer::timeout, &app, [&] {
        if (settings.autoRepair())
            runRepair(false);
    });
    QObject::connect(&tray, &TrayIcon::repairRequested, &app, [&] { runRepair(true); });

    // Live activity / settings panel (lazy-created once).
    std::unique_ptr<ActivityPanel> activityPanel;
    auto showActivity = [&](bool show) {
        if (show && !activityPanel) {
            activityPanel = std::make_unique<ActivityPanel>(&core, &settings);
            QObject::connect(activityPanel.get(), &ActivityPanel::closed, &app, [&] {
                settings.setActivityPanel(false);
                settings.sync();
            });
        }
        if (activityPanel) {
            activityPanel->setVisible(show);
            if (show)
                activityPanel->raise();
        }
        settings.setActivityPanel(show);
        settings.sync();
    };
    QObject::connect(&tray, &TrayIcon::activityPanelToggled, &app,
                     [&showActivity](bool on) { showActivity(on); });

    QObject::connect(&tray, &TrayIcon::autostartToggled, &app, [&settings](bool on) {
        settings.setAutostart(on);
        settings.sync();
        applyAutostart(on);
    });

    QObject::connect(&tray, &TrayIcon::quitRequested, &app, &QApplication::quit);
    QObject::connect(&tray, &TrayIcon::settingsRequested, &app, [&] {
        SettingsDialog dlg(&settings);
        if (dlg.exec() == QDialog::Accepted) {
            core.setMaxEntries(dlg.maxEntries());
            core.setPollIntervalMs(dlg.pollIntervalMs());
            core.setRestoreDelayMs(dlg.restoreDelayMs());
            core.setRestoreAfterPaste(dlg.restoreAfterPaste());

            settings.setMaxEntries(dlg.maxEntries());
            settings.setPollIntervalMs(dlg.pollIntervalMs());
            settings.setRestoreDelayMs(dlg.restoreDelayMs());
            settings.setRestoreAfterPaste(dlg.restoreAfterPaste());
            settings.setAutoRepair(dlg.autoRepair());
            settings.setActivityPanel(dlg.activityPanel());
            const bool autostart = dlg.autostart();
            settings.setAutostart(autostart);
            settings.sync();
            applyAutostart(autostart);
        }
    });

    QObject::connect(&core, &MultipasteCore::manualPasteHint, &tray, [&tray](const QString &desc) {
        tray.showMessage(QStringLiteral("MultiPaste"),
                         QStringLiteral("Loaded %1 - press Ctrl+V to paste it.").arg(desc));
    });
    QObject::connect(&core, &MultipasteCore::errorReported, &tray,
                     [&tray](const QString &m) { tray.showMessage(QStringLiteral("MultiPaste"), m); });

    applyAutostart(settings.autostart());

    tray.show();
    core.start();
    qInfo("MultiPaste: backend=%s (%d items, poll %d ms)",
          qUtf8Printable(core.backendName()), core.count(), core.pollIntervalMs());

    if (settings.activityPanel())
        showActivity(true);

    if (platform == QLatin1String("wayland")) {
        QTimer::singleShot(800, &tray, [&tray, &settings] {
            tray.showMessage(QStringLiteral("MultiPaste"),
                             QStringLiteral("Trigger: %1 (System Settings -> Shortcuts). "
                                            "Alt+Right mouse works only on X11/XWayland.")
                                 .arg(settings.shortcut()));
        });
    }

    const int rc = app.exec();

    applyAutostart(settings.autostart());
    return rc;
}