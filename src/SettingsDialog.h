#pragma once

#include <QDialog>

class QCheckBox;
class QSpinBox;

namespace multipaste {

class Settings;

// Small dialog for the tunable knobs. Everything else (triggers) is explained
// in read-only labels, and the global shortcut itself is user-editable in
// System Settings -> Shortcuts (component "multipaste").
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(const Settings *settings, QWidget *parent = nullptr);

    int maxEntries() const;
    int pollIntervalMs() const;
    int restoreDelayMs() const;
    bool restoreAfterPaste() const;
    bool autostart() const;
    bool autoRepair() const;
    bool activityPanel() const;

private:
    QSpinBox *m_maxEntries = nullptr;
    QSpinBox *m_pollIntervalMs = nullptr;
    QSpinBox *m_restoreDelayMs = nullptr;
    QCheckBox *m_restoreAfterPaste = nullptr;
    QCheckBox *m_autostart = nullptr;
    QCheckBox *m_autoRepair = nullptr;
    QCheckBox *m_activityPanel = nullptr;
};

} // namespace multipaste