#include "HistoryItem.h"

#include <QGuiApplication>
#include <QMimeData>
#include <QtTest>

using namespace multipaste;

class TestHistory : public QObject
{
    Q_OBJECT

private slots:
    void deepCopyKeepsAllFormats();
    void classification();
    void fingerprintEquality();
    void fingerprintIncludesAllFormats();
};

void TestHistory::deepCopyKeepsAllFormats()
{
    auto src = std::make_unique<QMimeData>();
    src->setText(QStringLiteral("hello"));
    src->setData(QStringLiteral("application/x-custom"), QByteArrayLiteral("\x01\x02\x03"));

    auto copy = HistoryItem::deepCopy(src.get());
    QVERIFY(copy);
    QCOMPARE(copy->text(), QStringLiteral("hello"));
    QCOMPARE(copy->data(QStringLiteral("application/x-custom")), QByteArrayLiteral("\x01\x02\x03"));
    QVERIFY(copy->formats().contains(QStringLiteral("text/plain")));
    QVERIFY(copy->formats().contains(QStringLiteral("application/x-custom")));

    // Mutating the source must not affect the copy.
    src->setText(QStringLiteral("goodbye"));
    QCOMPARE(copy->text(), QStringLiteral("hello"));
}

void TestHistory::classification()
{
    {
        auto d = std::make_unique<QMimeData>();
        d->setText(QStringLiteral("hi"));
        HistoryItem item(std::move(d));
        QCOMPARE(item.description(), QStringLiteral("TEXT"));
    }
    {
        auto d = std::make_unique<QMimeData>();
        d->setData(QStringLiteral("image/png"), QByteArrayLiteral("png-bytes"));
        HistoryItem item(std::move(d));
        QCOMPARE(item.description(), QStringLiteral("IMAGE"));
    }
    {
        auto d = std::make_unique<QMimeData>();
        d->setHtml(QStringLiteral("<b>x</b>"));
        HistoryItem item(std::move(d));
        QCOMPARE(item.description(), QStringLiteral("HTML"));
    }
    {
        auto d = std::make_unique<QMimeData>();
        d->setData(QStringLiteral("text/uri-list"), QByteArray("https://kde.org/\r\n"));
        HistoryItem item(std::move(d));
        QCOMPARE(item.description(), QStringLiteral("URL"));
    }
    {
        auto d = std::make_unique<QMimeData>();
        d->setData(QStringLiteral("x-special/gnome-copied-files"),
                   QByteArrayLiteral("copy\nfile:///tmp/a.txt"));
        HistoryItem item(std::move(d));
        QCOMPARE(item.description(), QStringLiteral("FILES"));
    }
    {
        auto d = std::make_unique<QMimeData>();
        d->setData(QStringLiteral("application/octet-stream"), QByteArrayLiteral("\xde\xad"));
        HistoryItem item(std::move(d));
        QCOMPARE(item.description(), QStringLiteral("BINARY"));
    }
}

void TestHistory::fingerprintEquality()
{
    auto a = std::make_unique<QMimeData>();
    a->setText(QStringLiteral("same"));
    auto b = std::make_unique<QMimeData>();
    b->setText(QStringLiteral("same"));
    auto c = std::make_unique<QMimeData>();
    c->setText(QStringLiteral("different"));

    HistoryItem ha(std::move(a));
    HistoryItem hb(std::move(b));
    HistoryItem hc(std::move(c));

    QCOMPARE(ha.fingerprint(), hb.fingerprint());
    QVERIFY(ha.fingerprint() != hc.fingerprint());

    QVERIFY(ha.approxBytes() > 0);
    QCOMPARE(ha.approxBytes(), hb.approxBytes());
}

void TestHistory::fingerprintIncludesAllFormats()
{
    auto a = std::make_unique<QMimeData>();
    a->setText(QStringLiteral("x"));
    auto b = std::make_unique<QMimeData>();
    b->setText(QStringLiteral("x"));
    b->setData(QStringLiteral("application/x-extra"), QByteArrayLiteral("y"));

    HistoryItem ha(std::move(a));
    HistoryItem hb(std::move(b));
    QVERIFY(ha.fingerprint() != hb.fingerprint());
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    TestHistory tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_history.moc"