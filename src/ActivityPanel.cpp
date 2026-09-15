#include "ActivityPanel.h"

#include "MultipasteCore.h"
#include "Settings.h"

#include <QCheckBox>
#include <QBrush>
#include <QCloseEvent>
#include <QColor>
#include <QDateTime>
#include <QFont>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

namespace multipaste {

ActivityPanel::ActivityPanel(MultipasteCore *core, Settings *settings, QWidget *parent)
    : QWidget(parent)
    , m_core(core)
    , m_settings(settings)
{
    setWindowTitle(QStringLiteral("MultiPaste - Activity"));
    setWindowIcon(QIcon::fromTheme(QStringLiteral("edit-paste")));

    auto v = new QVBoxLayout(this);

    auto *splitter = new QSplitter(Qt::Horizontal, this);

    auto *listBox = new QWidget(splitter);
    auto *lv = new QVBoxLayout(listBox);
    lv->setContentsMargins(0, 0, 0, 0);
    lv->addWidget(new QLabel(QStringLiteral("Sequential copies (next to paste highlighted)"), listBox));
    m_list = new QListWidget(listBox);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    lv->addWidget(m_list);

    auto *logBox = new QWidget(splitter);
    auto *logv = new QVBoxLayout(logBox);
    logv->setContentsMargins(0, 0, 0, 0);
    logv->addWidget(new QLabel(QStringLiteral("Copy / paste activity"), logBox));
    m_log = new QPlainTextEdit(logBox);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(qMax(200, m_settings->activityLogSize()));
    m_log->setLineWrapMode(QPlainTextEdit::NoWrap);
    logv->addWidget(m_log);

    splitter->addWidget(listBox);
    splitter->addWidget(logBox);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);
    v->addWidget(splitter, 1);

    auto *controls = new QHBoxLayout;
    m_enabledCheck = new QCheckBox(QStringLiteral("Monitoring enabled"), this);
    m_enabledCheck->setChecked(m_core->enabled());
    m_pasteButton = new QPushButton(QStringLiteral("Paste Next"), this);
    auto *newSeq = new QPushButton(QStringLiteral("New Sequence"), this);
    auto *clear = new QPushButton(QStringLiteral("Clear Sequence"), this);
    m_syncButton = new QPushButton(QStringLiteral("Repair / Re-sync"), this);
    m_syncButton->setToolTip(QStringLiteral("Re-read the clipboard and restart the capture poll.\n"
                                            "Also re-arms the X11 mouse trigger and global shortcut."));
    auto *copyBtn = new QPushButton(QStringLiteral("Copy Selected"), this);
    auto *closeBtn = new QPushButton(QStringLiteral("Close"), this);

    controls->addWidget(m_enabledCheck);
    controls->addWidget(m_pasteButton);
    controls->addWidget(newSeq);
    controls->addWidget(clear);
    controls->addWidget(copyBtn);
    controls->addStretch(1);
    controls->addWidget(m_syncButton);
    controls->addWidget(closeBtn);
    v->addLayout(controls);

    connect(m_enabledCheck, &QCheckBox::toggled, m_core, &MultipasteCore::setEnabled);
    connect(m_core, &MultipasteCore::itemsChanged, this, &ActivityPanel::onItemsChanged);
    connect(m_core, &MultipasteCore::positionChanged, this, &ActivityPanel::onPositionChanged);
    connect(m_core, &MultipasteCore::logEntry, this, &ActivityPanel::onLogEntry);
    connect(m_pasteButton, &QPushButton::clicked, m_core, &MultipasteCore::pasteNext);
    connect(newSeq, &QPushButton::clicked, m_core, &MultipasteCore::newSequence);
    connect(clear, &QPushButton::clicked, m_core, &MultipasteCore::clearSequence);
    connect(copyBtn, &QPushButton::clicked, this, &ActivityPanel::copySelectionToClipboard);
    connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);
    connect(m_core, &MultipasteCore::enabledChanged, this, [this](bool on) {
        if (m_enabledCheck->isChecked() != on)
            m_enabledCheck->setChecked(on);
    });

    resize(760, 460);
    refresh();
}

ActivityPanel::~ActivityPanel() = default;

void ActivityPanel::refresh()
{
    rebuildList();
    const QStringList log = m_core->activityLog();
    m_log->clear();
    for (auto it = log.crbegin(); it != log.crend(); ++it)
        m_log->appendPlainText(*it);
}

void ActivityPanel::closeEvent(QCloseEvent *event)
{
    emit closed();
    QWidget::closeEvent(event);
}

void ActivityPanel::onItemsChanged()
{
    rebuildList();
}

void ActivityPanel::onPositionChanged()
{
    rebuildList();
}

void ActivityPanel::onLogEntry(const QString &line)
{
    m_log->appendPlainText(line);
}

void ActivityPanel::rebuildList()
{
    const QStringList items = m_core->descriptions();
    const int next = m_core->nextIndex();

    m_list->clear();
    for (int i = 0; i < items.size(); ++i) {
        auto *it = new QListWidgetItem(QStringLiteral("%1. %2").arg(i + 1).arg(items.at(i)), m_list);
        it->setData(Qt::UserRole, i);
        QFont f = it->font();
        f.setBold(i == next);
        it->setFont(f);
        if (i == next)
            it->setForeground(QBrush(QColor(QStringLiteral("#2a88ff"))));
    }
    if (m_list->count() > 0 && next >= 0 && next < m_list->count())
        m_list->setCurrentRow(next);
}

void ActivityPanel::copySelectionToClipboard()
{
    const int row = m_list->currentRow();
    if (row < 0)
        return;
    auto *item = m_core->itemAt(row);
    if (!item)
        return;
    QGuiApplication::clipboard()->setMimeData(HistoryItem::deepCopy(item->mime()).release());
}

} // namespace multipaste