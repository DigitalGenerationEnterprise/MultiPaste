#pragma once

#include <QObject>
#include <QStringList>

namespace multipaste {

class MultipasteCore;

// Session-bus service for the Plasma 6 widget (and any external tool):
//
//   bus:        org.multipaste.MultiPaste
//   object:     /MultiPaste
//   properties: Items (QStringList), NextIndex (int), Enabled (bool),
//               Activity (QStringList), StatusLine (QString)
//   methods:    PasteNext(), NewSequence(), ClearSequence(),
//               SetEnabled(bool), Refresh()
//   signals:    ItemsChanged(), PositionChanged(), EnabledChanged(bool),
//               LogEntry(s)
class MultipasteDbusServer : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.multipaste.MultiPaste")
    Q_PROPERTY(QStringList Items READ items NOTIFY ItemsChanged)
    Q_PROPERTY(int NextIndex READ nextIndex NOTIFY PositionChanged)
    Q_PROPERTY(bool Enabled READ enabled NOTIFY EnabledChanged)
    Q_PROPERTY(QStringList Activity READ activity NOTIFY LogEntry)
    Q_PROPERTY(QString StatusLine READ statusLine NOTIFY ItemsChanged)

public:
    explicit MultipasteDbusServer(MultipasteCore *core, QObject *parent = nullptr);

    QStringList items() const;
    int nextIndex() const;
    bool enabled() const;
    QStringList activity() const;
    QString statusLine() const;

    bool isRegistered() const
    {
        return m_registered;
    }

public slots:
    void PasteNext();
    void NewSequence();
    void ClearSequence();
    void SetEnabled(bool enabled);
    void Refresh();

signals:
    void ItemsChanged();
    void PositionChanged();
    void EnabledChanged(bool enabled);
    void LogEntry(const QString &line);

private:
    MultipasteCore *m_core = nullptr;
    bool m_registered = false;
};

} // namespace multipaste