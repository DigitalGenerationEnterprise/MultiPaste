#include "SettingsDialog.h"

#include "Settings.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace multipaste {

SettingsDialog::SettingsDialog(const Settings *s, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("MultiPaste Settings"));

    m_maxEntries = new QSpinBox(this);
    m_maxEntries->setRange(1, 200);
    m_maxEntries->setValue(s->maxEntries());

    m_maxCaptureChars = new QSpinBox(this);
    m_maxCaptureChars->setRange(10, 100000);
    m_maxCaptureChars->setSingleStep(100);
    m_maxCaptureChars->setSuffix(QStringLiteral(" chars"));
    m_maxCaptureChars->setValue(s->maxCaptureChars());
    m_maxCaptureChars->setToolTip(QStringLiteral(
        "Copies longer than this (e.g. big terminal dumps) are skipped so the "
        "paste queue only holds short snippets. Set it very high to keep "
        "everything."));

    m_pollIntervalMs = new QSpinBox(this);
    m_pollIntervalMs->setRange(500, 10000);
    m_pollIntervalMs->setSingleStep(100);
    m_pollIntervalMs->setSuffix(QStringLiteral(" ms"));
    m_pollIntervalMs->setValue(s->pollIntervalMs());

    m_restoreDelayMs = new QSpinBox(this);
    m_restoreDelayMs->setRange(0, 2000);
    m_restoreDelayMs->setSingleStep(50);
    m_restoreDelayMs->setSuffix(QStringLiteral(" ms"));
    m_restoreDelayMs->setValue(s->restoreDelayMs());

    m_restoreAfterPaste = new QCheckBox(QStringLiteral("Restore previous clipboard after automatic paste"), this);
    m_restoreAfterPaste->setChecked(s->restoreAfterPaste());

    m_autostart = new QCheckBox(QStringLiteral("Start MultiPaste when I log in"), this);
    m_autostart->setChecked(s->autostart());

    m_autoRepair = new QCheckBox(QStringLiteral("Self-repair: re-register triggers and re-read the clipboard automatically"), this);
    m_autoRepair->setChecked(s->autoRepair());
    m_autoRepair->setToolTip(QStringLiteral(
        "Every 30 seconds MultiPaste checks the global shortcut and the X11 "
        "mouse grab, re-issues them if they were lost, and re-reads the "
        "clipboard if a Wayland transfer was missed."));

    m_activityPanel = new QCheckBox(QStringLiteral("Show the Activity panel when MultiPaste starts"), this);
    m_activityPanel->setChecked(s->activityPanel());
    m_activityPanel->setToolTip(QStringLiteral(
        "The Activity panel is a small window that shows everything being "
        "copied and pasted, and which item comes next."));

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("Max history entries"), m_maxEntries);
    form->addRow(QStringLiteral("Skip copies longer than"), m_maxCaptureChars);
    form->addRow(QStringLiteral("Clipboard poll interval"), m_pollIntervalMs);
    form->addRow(QStringLiteral("Restore delay"), m_restoreDelayMs);
    form->addRow(QString(), m_restoreAfterPaste);
    form->addRow(QString(), m_autostart);
    form->addRow(QString(), m_autoRepair);
    form->addRow(QString(), m_activityPanel);

    const QString platform = QGuiApplication::platformName();
    const QString hint = (platform == QLatin1String("wayland"))
        ? QStringLiteral(
              "Session: Wayland\n"
              "Trigger: KDE global shortcut (see System Settings -> Shortcuts, "
              "component \"multipaste\"). Alt+Right mouse works only for "
              "XWayland applications.")
        : QStringLiteral(
              "Session: X11\n"
              "Trigger: hold Alt and click the Right mouse button; a KDE "
              "global shortcut is also registered.\n"
              "Tip: install the Plasma widget \"MultiPaste Activity\" from "
              "~/.local/share/plasma/plasmoids to see copies/pastes on the "
              "desktop.");
    auto *info = new QLabel(hint, this);
    info->setWordWrap(true);
    info->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *infoFrame = new QFrame(this);
    infoFrame->setFrameShape(QFrame::StyledPanel);
    auto *infoLayout = new QVBoxLayout(infoFrame);
    infoLayout->addWidget(info);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(infoFrame);
    layout->addWidget(buttons);
    setMinimumWidth(460);
}

int SettingsDialog::maxEntries() const
{
    return m_maxEntries->value();
}

int SettingsDialog::maxCaptureChars() const
{
    return m_maxCaptureChars->value();
}

int SettingsDialog::pollIntervalMs() const
{
    return m_pollIntervalMs->value();
}

int SettingsDialog::restoreDelayMs() const
{
    return m_restoreDelayMs->value();
}

bool SettingsDialog::restoreAfterPaste() const
{
    return m_restoreAfterPaste->isChecked();
}

bool SettingsDialog::autostart() const
{
    return m_autostart->isChecked();
}

bool SettingsDialog::autoRepair() const
{
    return m_autoRepair->isChecked();
}

bool SettingsDialog::activityPanel() const
{
    return m_activityPanel->isChecked();
}

} // namespace multipaste