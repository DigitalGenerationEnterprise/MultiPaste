#include "MultipasteCore.h"
#include "PasteLifter.h"

#include <QApplication>
#include <QClipboard>
#include <QtTest>

using namespace multipaste;

namespace {

// PasteLifter stand-in that claims it injected the paste, so the restore path
// in the core can be tested without touching a real X server.
class FakeLifter : public PasteLifter
{
public:
    explicit FakeLifter(QObject *parent = nullptr)
        : PasteLifter(parent)
    {
    }

    bool paste() override
    {
        ++pasteCount;
        return true;
    }

    bool isAutomatic() const override
    {
        return true;
    }

    int pasteCount = 0;
};

} // namespace

class TestCore : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void storesInCopyOrder();
    void dedupsConsecutiveDuplicates();
    void ignoresOwnWritesAndManualPaste();
    void wrapsAfterLastItem();
    void newSequenceResetsPosition();
    void clearSequenceEmptiesStore();
    void capsAtMaxEntries();
    void continuesAfterNewCopies();
    void restoresPreviousClipboard();
    void seedSuppressesPreviousRunContent();

private:
    QString copyText(const QString &t);
    void run();
    QString clipboardText() const;

    MultipasteCore *m_core = nullptr;
};

void TestCore::initTestCase()
{
    m_core = new MultipasteCore(this);
    m_core->setMaxEntries(30);
    m_core->setRestoreDelayMs(0);
    m_core->start();
    run();
}

void TestCore::run()
{
    QGuiApplication::processEvents();
    QTest::qWait(60); // let scheduled captures fire
    QGuiApplication::processEvents();
}

QString TestCore::copyText(const QString &t)
{
    QGuiApplication::clipboard()->setText(t);
    run();
    return t;
}

QString TestCore::clipboardText() const
{
    return QGuiApplication::clipboard()->text();
}

void TestCore::storesInCopyOrder()
{
    m_core->clearSequence();
    copyText(QStringLiteral("sA"));
    copyText(QStringLiteral("sB"));
    copyText(QStringLiteral("sC"));

    QCOMPARE(m_core->count(), 3);
    QCOMPARE(m_core->itemAt(0)->mime()->text(), QStringLiteral("sA"));
    QCOMPARE(m_core->itemAt(1)->mime()->text(), QStringLiteral("sB"));
    QCOMPARE(m_core->itemAt(2)->mime()->text(), QStringLiteral("sC"));
    QCOMPARE(m_core->statusLine(), QStringLiteral("3 items"));
    QCOMPARE(m_core->positionLine(), QStringLiteral("1 / 3"));
}

void TestCore::dedupsConsecutiveDuplicates()
{
    m_core->clearSequence();
    copyText(QStringLiteral("dA"));
    copyText(QStringLiteral("dA"));
    copyText(QStringLiteral("dB"));
    QCOMPARE(m_core->count(), 2);
}

void TestCore::ignoresOwnWritesAndManualPaste()
{
    m_core->clearSequence();
    copyText(QStringLiteral("iA"));
    copyText(QStringLiteral("iB"));
    QCOMPARE(m_core->count(), 2);

    // No lifter => the item is loaded and the user pastes manually.
    m_core->setPasteLifter(nullptr);
    m_core->pasteNext();
    run();

    QCOMPARE(clipboardText(), QStringLiteral("iA"));
    QCOMPARE(m_core->count(), 2);                 // our temporary write must not be stored
    QCOMPARE(m_core->nextIndex(), 1);
    QCOMPARE(m_core->positionLine(), QStringLiteral("2 / 2"));

    m_core->pasteNext();
    run();
    QCOMPARE(clipboardText(), QStringLiteral("iB"));
    QCOMPARE(m_core->count(), 2);
}

void TestCore::wrapsAfterLastItem()
{
    m_core->clearSequence();
    copyText(QStringLiteral("wX"));
    copyText(QStringLiteral("wY"));

    m_core->setPasteLifter(nullptr);
    m_core->newSequence();
    m_core->pasteNext(); // X
    run();
    m_core->pasteNext(); // Y -> wraps the position back to the first item
    run();
    QCOMPARE(m_core->nextIndex(), 0);
    QCOMPARE(m_core->positionLine(), QStringLiteral("1 / 2"));
}

void TestCore::newSequenceResetsPosition()
{
    m_core->clearSequence();
    copyText(QStringLiteral("nA"));
    copyText(QStringLiteral("nB"));
    m_core->setPasteLifter(nullptr);
    m_core->pasteNext();
    run();
    QCOMPARE(m_core->nextIndex(), 1);
    m_core->newSequence();
    QCOMPARE(m_core->nextIndex(), 0);
    QCOMPARE(m_core->count(), 2); // history preserved
}

void TestCore::clearSequenceEmptiesStore()
{
    m_core->clearSequence();
    copyText(QStringLiteral("cA"));
    QCOMPARE(m_core->count(), 1);
    m_core->clearSequence();
    QCOMPARE(m_core->count(), 0);
    QCOMPARE(m_core->nextIndex(), -1);
    QCOMPARE(m_core->positionLine(), QStringLiteral("no items"));
}

void TestCore::capsAtMaxEntries()
{
    m_core->setMaxEntries(2);
    m_core->clearSequence();
    copyText(QStringLiteral("pA"));
    copyText(QStringLiteral("pB"));
    copyText(QStringLiteral("pC"));
    QCOMPARE(m_core->count(), 2);
    QCOMPARE(m_core->itemAt(0)->mime()->text(), QStringLiteral("pB"));
    QCOMPARE(m_core->itemAt(1)->mime()->text(), QStringLiteral("pC"));
    m_core->setMaxEntries(30);
}

void TestCore::continuesAfterNewCopies()
{
    m_core->clearSequence();
    copyText(QStringLiteral("tA"));
    copyText(QStringLiteral("tB"));
    copyText(QStringLiteral("tC"));
    m_core->setPasteLifter(nullptr);
    m_core->newSequence();
    m_core->pasteNext(); // A
    run();
    m_core->pasteNext(); // B
    run();
    copyText(QStringLiteral("tD")); // user copies more mid-sequence
    QCOMPARE(m_core->count(), 4);
    // Position must continue FIFO: C is next, D after it.
    QCOMPARE(m_core->nextIndex(), 2);
    m_core->pasteNext();
    run();
    QCOMPARE(clipboardText(), QStringLiteral("tC"));
    QCOMPARE(m_core->nextIndex(), 3);
    m_core->pasteNext();
    run();
    QCOMPARE(clipboardText(), QStringLiteral("tD"));
    QCOMPARE(m_core->nextIndex(), 0); // wrapped
}

void TestCore::restoresPreviousClipboard()
{
    m_core->clearSequence();
    copyText(QStringLiteral("rA"));
    copyText(QStringLiteral("rB"));

    auto lifter = new FakeLifter(m_core);
    m_core->setPasteLifter(lifter);
    m_core->pasteNext(); // pastes A automatically, should restore B
    run();

    QCOMPARE(lifter->pasteCount, 1);
    QCOMPARE(clipboardText(), QStringLiteral("rB")); // user's previous clipboard back
    QCOMPARE(m_core->count(), 2);                   // no extra history entries
}

void TestCore::seedSuppressesPreviousRunContent()
{
    m_core->clearSequence();
    copyText(QStringLiteral("zA"));
    QCOMPARE(m_core->count(), 1);
    const QByteArray fp = m_core->itemAt(0)->fingerprint();

    // A restart: the history is gone but the clipboard still holds "zA".
    m_core->clearSequence();
    m_core->seedFromFingerprint(fp);
    copyText(QStringLiteral("zA")); // must NOT re-enter the history
    QCOMPARE(m_core->count(), 0);
    QCOMPARE(m_core->nextIndex(), -1);

    // Anything else is captured normally from now on.
    copyText(QStringLiteral("zB"));
    QCOMPARE(m_core->count(), 1);
    QCOMPARE(m_core->itemAt(0)->mime()->text(), QStringLiteral("zB"));
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    TestCore tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_core.moc"