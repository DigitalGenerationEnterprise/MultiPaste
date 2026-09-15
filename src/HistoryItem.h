#pragma once

#include <QMimeData>
#include <QSet>
#include <QString>
#include <QStringList>
#include <memory>

namespace multipaste {

// Stable fingerprint of a QMimeData payload (sorted MIME formats + data).
// Optionally returns the total payload size in bytes.
QByteArray fingerprintOf(const QMimeData *data, qint64 *approxBytes = nullptr);

// One captured clipboard entry. Deep-copies the full QMimeData so the entry
// stays valid even after the original clipboard owner releases the selection.
class HistoryItem
{
public:
    HistoryItem() = default;
    HistoryItem(HistoryItem &&) noexcept = default;
    HistoryItem &operator=(HistoryItem &&) noexcept = default;
    HistoryItem(const HistoryItem &) = delete;
    HistoryItem &operator=(const HistoryItem &) = delete;

    explicit HistoryItem(std::unique_ptr<QMimeData> data)
        : m_data(std::move(data))
    {
    }

    bool isEmpty() const
    {
        return !m_data;
    }

    const QMimeData *mime() const
    {
        return m_data.get();
    }

    QStringList formats() const
    {
        return m_data ? m_data->formats() : QStringList();
    }

    // Short human-friendly label: "TEXT", "IMAGE", "URL", "HTML", "FILES", ...
    QString description() const;

    // Stable fingerprint of the payload, used for dedup and for ignoring our
    // own temporary clipboard writes. Computed lazily and cached.
    QByteArray fingerprint() const;

    // Approximate payload size in bytes (computed while fingerprinting).
    qint64 approxBytes() const
    {
        return m_approxBytes;
    }

    static std::unique_ptr<QMimeData> deepCopy(const QMimeData *src);

private:
    static qint64 payloadSize(const QMimeData *d);

    mutable std::unique_ptr<QMimeData> m_data;
    mutable QByteArray m_fingerprint;
    mutable bool m_fingerprintValid = false;
    mutable qint64 m_approxBytes = 0;
};

} // namespace multipaste