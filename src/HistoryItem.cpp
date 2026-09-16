#include "HistoryItem.h"

#include <QCryptographicHash>
#include <QMimeData>
#include <utility>

namespace multipaste {

namespace {
bool hasPrefix(const QStringList &formats, const QString &prefix)
{
    for (const QString &f : formats) {
        if (f.startsWith(prefix))
            return true;
    }
    return false;
}
} // namespace

QByteArray fingerprintOf(const QMimeData *data, qint64 *approxBytes)
{
    QByteArray result;
    qint64 bytes = 0;
    if (data) {
        QStringList fmts = data->formats();
        fmts.removeDuplicates();
        fmts.sort();

        QCryptographicHash acc(QCryptographicHash::Md5);
        for (const QString &fmt : std::as_const(fmts)) {
            const QByteArray payload = data->data(fmt);
            bytes += payload.size();
            acc.addData(fmt.toUtf8());
            acc.addData(QByteArray(1, '\0'));
            acc.addData(payload);
            acc.addData(QByteArray(1, '\0'));
        }
        result = acc.result().toHex();
    }
    if (approxBytes)
        *approxBytes = bytes;
    return result;
}

std::unique_ptr<QMimeData> HistoryItem::deepCopy(const QMimeData *src)
{
    if (!src)
        return {};
    auto copy = std::make_unique<QMimeData>();
    const QStringList formats = src->formats();
    for (const QString &fmt : formats) {
        const QByteArray data = src->data(fmt);
        if (!data.isNull())
            copy->setData(fmt, data);
    }
    return copy;
}

qint64 HistoryItem::payloadSize(const QMimeData *d)
{
    qint64 total = 0;
    const QStringList formats = d->formats();
    for (const QString &fmt : formats)
        total += d->data(fmt).size();
    return total;
}

QString HistoryItem::description() const
{
    if (!m_data)
        return QStringLiteral("EMPTY");

    const QStringList formats = m_data->formats();

    if (hasPrefix(formats, QStringLiteral("image/")))
        return QStringLiteral("IMAGE");
    if (formats.contains(QStringLiteral("x-special/gnome-copied-files"))
        || formats.contains(QStringLiteral("x-kde-cutselection"))
        || hasPrefix(formats, QStringLiteral("application/x-kde")))
        return QStringLiteral("FILES");
    if (formats.contains(QStringLiteral("text/html"))
        || formats.contains(QStringLiteral("application/xhtml+xml")))
        return QStringLiteral("HTML");
    if (formats.contains(QStringLiteral("text/uri-list")))
        return QStringLiteral("URL");
    if (formats.contains(QStringLiteral("text/rtf")))
        return QStringLiteral("RTF");
    if (hasPrefix(formats, QStringLiteral("text/")))
        return QStringLiteral("TEXT");
    return QStringLiteral("BINARY");
}

QString HistoryItem::preview() const
{
    if (!m_data)
        return QStringLiteral("EMPTY");

    const QStringList formats = m_data->formats();
    QString s;
    if (formats.contains(QStringLiteral("text/uri-list")))
        s = QString::fromUtf8(m_data->data(QStringLiteral("text/uri-list")))
                .split(u'\n')
                .value(0)
                .trimmed();
    else if (formats.contains(QStringLiteral("text/plain"))
             || formats.contains(QStringLiteral("text/plain;charset=utf-8")))
        s = m_data->text();

    if (s.trimmed().isEmpty())
        return description(); // no readable text (images, binaries, files) - keep the type label

    s = s.trimmed().simplified();
    if (s.length() > 40)
        s = s.left(37) + QStringLiteral("…");
    return s;
}

QByteArray HistoryItem::fingerprint() const
{
    if (m_fingerprintValid)
        return m_fingerprint;

    if (!m_data) {
        m_fingerprintValid = true;
        return m_fingerprint;
    }

    m_fingerprint = fingerprintOf(m_data.get(), &m_approxBytes);
    m_fingerprintValid = true;
    return m_fingerprint;
}

} // namespace multipaste