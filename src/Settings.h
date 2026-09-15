#pragma once

#include <QSettings>
#include <QString>

namespace multipaste {

// Thin wrapper over QSettings so tests never depend on the real app config.
class Settings : public QObject
{
public:
    explicit Settings(QObject *parent = nullptr)
        : QObject(parent)
    {
        m_settings.setDefaultFormat(QSettings::IniFormat);
        m_settings.beginGroup(QStringLiteral("general"));
    }

    int maxEntries() const
    {
        return m_settings.value(QStringLiteral("maxEntries"), 30).toInt();
    }
    void setMaxEntries(int n)
    {
        m_settings.setValue(QStringLiteral("maxEntries"), n);
    }

    int pollIntervalMs() const
    {
        return m_settings.value(QStringLiteral("pollIntervalMs"), 2000).toInt();
    }
    void setPollIntervalMs(int ms)
    {
        m_settings.setValue(QStringLiteral("pollIntervalMs"), ms);
    }

    int restoreDelayMs() const
    {
        return m_settings.value(QStringLiteral("restoreDelayMs"), 200).toInt();
    }
    void setRestoreDelayMs(int ms)
    {
        m_settings.setValue(QStringLiteral("restoreDelayMs"), ms);
    }

    bool restoreAfterPaste() const
    {
        return m_settings.value(QStringLiteral("restoreAfterPaste"), true).toBool();
    }
    void setRestoreAfterPaste(bool b)
    {
        m_settings.setValue(QStringLiteral("restoreAfterPaste"), b);
    }

    bool enabled() const
    {
        return m_settings.value(QStringLiteral("enabled"), true).toBool();
    }
    void setEnabled(bool b)
    {
        m_settings.setValue(QStringLiteral("enabled"), b);
    }

    bool autostart() const
    {
        return m_settings.value(QStringLiteral("autostart"), true).toBool();
    }
    void setAutostart(bool b)
    {
        m_settings.setValue(QStringLiteral("autostart"), b);
    }

    bool x11MouseTrigger() const
    {
        return m_settings.value(QStringLiteral("x11MouseTrigger"), true).toBool();
    }
    void setX11MouseTrigger(bool b)
    {
        m_settings.setValue(QStringLiteral("x11MouseTrigger"), b);
    }

    QString shortcut() const
    {
        return m_settings.value(QStringLiteral("shortcut"), QLatin1String("Alt+Shift+V,Meta+Shift+V")).toString();
    }
    void setShortcut(const QString &s)
    {
        m_settings.setValue(QStringLiteral("shortcut"), s);
    }

    // Re-register the global shortcut / re-arm the X11 grab periodically,
    // and re-read the clipboard if a Wayland transfer was missed.
    bool autoRepair() const
    {
        return m_settings.value(QStringLiteral("autoRepair"), true).toBool();
    }
    void setAutoRepair(bool b)
    {
        m_settings.setValue(QStringLiteral("autoRepair"), b);
    }

    // Settings for the live activity / history window.
    bool activityPanel() const
    {
        return m_settings.value(QStringLiteral("activityPanel"), false).toBool();
    }
    void setActivityPanel(bool b)
    {
        m_settings.setValue(QStringLiteral("activityPanel"), b);
    }

    int activityLogSize() const
    {
        return m_settings.value(QStringLiteral("activityLogSize"), 500).toInt();
    }
    void setActivityLogSize(int n)
    {
        m_settings.setValue(QStringLiteral("activityLogSize"), n);
    }

    void sync()
    {
        m_settings.sync();
    }

private:
    QSettings m_settings;
};

} // namespace multipaste