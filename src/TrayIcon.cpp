#include "TrayIcon.h"

#include "MultipasteCore.h"
#include "Settings.h"

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QPainter>
#include <QPixmap>

namespace multipaste {

namespace {
constexpr int kMaxVisibleItems = 8;
constexpr int kIconSize = 22;
} // namespace

TrayIcon::TrayIcon(MultipasteCore *core, Settings *settings, QObject *parent)
    : QObject(parent)
    , m_core(core)
    , m_settings(settings)
{
    m_icon.setIcon(makeIcon());

    connect(core, &MultipasteCore::itemsChanged, this, &TrayIcon::rebuildMenu);
    connect(core, &MultipasteCore::positionChanged, this, &TrayIcon::rebuildMenu);
    connect(&m_icon, &QSystemTrayIcon::activated, this, &TrayIcon::onActivated);

    rebuildMenu();
    m_icon.setToolTip(QStringLiteral("MultiPaste"));
}

QIcon TrayIcon::makeIcon() const
{
    QPixmap pm(kIconSize, kIconSize);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    // Clipboard board.
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x26, 0x7f, 0xb5));
    p.drawRoundedRect(3, 6, 16, 13, 2, 2);
    // Clip.
    p.setBrush(QColor(0x26, 0x7f, 0xb5));
    p.drawRoundedRect(7, 2, 8, 4, 1.5, 1.5);
    // "M" glyph.
    p.setPen(QPen(Qt::white, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawLine(QPointF(7, 15), QPointF(7, 10));
    p.drawLine(QPointF(7, 10), QPointF(11, 13));
    p.drawLine(QPointF(15, 15), QPointF(15, 10));
    p.drawLine(QPointF(7, 13), QPointF(15, 13));
    p.drawLine(QPointF(11, 13), QPointF(15, 10));
    p.end();
    return QIcon(pm);
}

QString TrayIcon::itemLabel(int index) const
{
    const HistoryItem *it = m_core->itemAt(index);
    if (!it)
        return QString();
    QString label = QStringLiteral("%1  %2").arg(index + 1).arg(it->description());
    if (label.size() > 40)
        label = label.left(37) + QStringLiteral("...");
    return label;
}

void TrayIcon::rebuildMenu()
{
    if (!m_menu)
        m_menu = new QMenu();
    else
        m_menu->clear();
    QMenu *menu = m_menu;

    QAction *status = menu->addAction(QStringLiteral("%1  -  next: %2")
                                          .arg(m_core->statusLine(), m_core->positionLine()));
    status->setEnabled(false);

    menu->addSeparator();
    const int shown = qMin(kMaxVisibleItems, m_core->count());
    for (int i = 0; i < shown; ++i) {
        QAction *a = menu->addAction(itemLabel(i));
        a->setEnabled(false);
    }
    if (m_core->count() > shown)
        menu->addAction(QStringLiteral("...and %1 more").arg(m_core->count() - shown))->setEnabled(false);

    menu->addSeparator();

    QAction *paste = menu->addAction(QStringLiteral("Paste Next"));
    paste->setEnabled(m_core->count() > 0 && m_core->enabled());
    connect(paste, &QAction::triggered, m_core, &MultipasteCore::pasteNext);

    QAction *enable = menu->addAction(QStringLiteral("Enable"));
    enable->setCheckable(true);
    enable->setChecked(m_core->enabled());
    connect(enable, &QAction::toggled, m_core, &MultipasteCore::setEnabled);

    connect(menu->addAction(QStringLiteral("New Sequence")), &QAction::triggered,
            m_core, &MultipasteCore::newSequence);
    connect(menu->addAction(QStringLiteral("Clear Sequence")), &QAction::triggered,
            m_core, &MultipasteCore::clearSequence);

    menu->addSeparator();

    QAction *activity = menu->addAction(QStringLiteral("Show Activity / Settings Panel"));
    activity->setCheckable(true);
    activity->setChecked(m_settings->activityPanel());
    connect(activity, &QAction::toggled, this, &TrayIcon::activityPanelToggled);

    QAction *repair = menu->addAction(QStringLiteral("Repair & Re-sync"));
    repair->setToolTip(QStringLiteral("Re-read the clipboard, re-register the global shortcut\n"
                                      "and re-arm the X11 mouse trigger."));
    connect(repair, &QAction::triggered, this, &TrayIcon::repairRequested);

    menu->addSeparator();

    connect(menu->addAction(QStringLiteral("Settings...")), &QAction::triggered,
            this, &TrayIcon::settingsRequested);

    QAction *autostart = menu->addAction(QStringLiteral("Start on Login"));
    autostart->setCheckable(true);
    autostart->setChecked(m_settings->autostart());
    connect(autostart, &QAction::toggled, this, &TrayIcon::autostartToggled);

    menu->addSeparator();
    connect(menu->addAction(QStringLiteral("Quit")), &QAction::triggered,
            this, &TrayIcon::quitRequested);

    m_icon.setContextMenu(menu);
}

void TrayIcon::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::MiddleClick) {
        // Pop up the same menu manually on left/middle click. On Wayland a
        // grabbing popup needs a focused transient parent, so only do this on
        // X11; the native tray context menu is the supported path on Wayland.
        if (QGuiApplication::platformName() == QLatin1String("xcb") && m_icon.contextMenu())
            m_icon.contextMenu()->popup(QCursor::pos());
    }
}

void TrayIcon::show()
{
    m_icon.show();
}

void TrayIcon::showMessage(const QString &title, const QString &body)
{
    m_icon.showMessage(title, body, QSystemTrayIcon::Information, 3500);
}

} // namespace multipaste