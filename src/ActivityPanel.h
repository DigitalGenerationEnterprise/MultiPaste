#pragma once

#include <QPlainTextEdit>
#include <QWidget>

class QCheckBox;
class QListWidget;
class QPushButton;

namespace multipaste {

class MultipasteCore;
class Settings;

// Live "settings & activity" panel: shows the sequence being copied/pasted
// (and what comes next), plus a rolling log of every copy, paste and repair.
class ActivityPanel : public QWidget
{
    Q_OBJECT

public:
    ActivityPanel(MultipasteCore *core, Settings *settings, QWidget *parent = nullptr);
    ~ActivityPanel() override;

    void refresh();

protected:
    void closeEvent(QCloseEvent *event) override;

signals:
    void closed();

private slots:
    void onItemsChanged();
    void onPositionChanged();
    void onLogEntry(const QString &line);

private:
    void rebuildList();
    void copySelectionToClipboard();

    MultipasteCore *m_core = nullptr;
    Settings *m_settings = nullptr;

    QListWidget *m_list = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QCheckBox *m_enabledCheck = nullptr;
    QPushButton *m_pasteButton = nullptr;
    QPushButton *m_syncButton = nullptr;
};

} // namespace multipaste