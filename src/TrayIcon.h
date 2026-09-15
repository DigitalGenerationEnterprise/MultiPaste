#pragma once

#include <QIcon>
#include <QObject>
#include <QSystemTrayIcon>

namespace multipaste {

class MultipasteCore;
class Settings;

// Minimal tray UI: icon + a single rebuildable context menu that shows the
// sequence state and the actions from the spec.
class TrayIcon : public QObject
{
    Q_OBJECT

public:
    TrayIcon(MultipasteCore *core, Settings *settings, QObject *parent = nullptr);

    void show();
    void showMessage(const QString &title, const QString &body);

signals:
    void settingsRequested();
    void autostartToggled(bool enabled);
    void activityPanelToggled(bool visible);
    void repairRequested();
    void quitRequested();

private slots:
    void rebuildMenu();
    void onActivated(QSystemTrayIcon::ActivationReason reason);

private:
    QIcon makeIcon() const;
    QString itemLabel(int index) const;

    MultipasteCore *m_core = nullptr;
    Settings *m_settings = nullptr;
    QSystemTrayIcon m_icon;
    QMenu *m_menu = nullptr;
};

} // namespace multipaste