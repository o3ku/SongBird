#include <QtTest>

#include "platform/SingleInstanceGuard.h"

#include <QCoreApplication>
#include <QString>
#include <QStringList>

// SingleInstanceGuard is the lock both front ends take at startup. Until now it had no test of
// its own: the main app exercised it only through tryAcquireStartupSingleInstance() with a
// lambda the StartupAdminElevationTests suite stubbed out, so the guard's own semantics --
// what a second instance of the SAME key sees, and what an unrelated key never sees --
// were pinned by nothing. SongBirdAuto now takes the same guard, which is what made the gap
// worth closing: the startup proxy recovery that guard protects is only sound while the
// second instance is refused.
class SingleInstanceGuardTests : public QObject {
    Q_OBJECT

private slots:
    void acquiresWhenNoOtherInstanceHoldsTheKey();
    void refusesASecondHolderOfTheSameKey();
    void allowsDifferentKeysSideBySide();
    void secondHolderCanAcquireAfterTheFirstReleases();
};

namespace {

// A key no other test in this binary can hold. Tests inside one Qt Test binary share a
// process, so a shared literal key would let one slot's guard refuse another's depending on
// execution order -- the failure would move with the slot name, the worst kind of flake.
QString uniqueGuardKey()
{
    static int counter = 0;
    return QStringLiteral("SongBird-test-guard-%1-%2")
        .arg(QCoreApplication::applicationPid())
        .arg(++counter);
}

} // namespace

void SingleInstanceGuardTests::acquiresWhenNoOtherInstanceHoldsTheKey()
{
    SingleInstanceGuard guard(uniqueGuardKey());
    QVERIFY(guard.tryAcquire());
}

void SingleInstanceGuardTests::refusesASecondHolderOfTheSameKey()
{
    const QString key = uniqueGuardKey();
    SingleInstanceGuard first(key);
    QVERIFY(first.tryAcquire());

    SingleInstanceGuard second(key);
    QVERIFY(!second.tryAcquire());
}

void SingleInstanceGuardTests::allowsDifferentKeysSideBySide()
{
    // The main app and SongBirdAuto run side by side on one machine, each guarding its own
    // kind. That arrangement is only sound if one key never interferes with the other, which
    // is also what makes the two executables' keys deliberately different.
    SingleInstanceGuard mainApp(QStringLiteral("SongBird-test-front-end-a"));
    SingleInstanceGuard autoApp(QStringLiteral("SongBird-test-front-end-b"));
    QVERIFY(mainApp.tryAcquire());
    QVERIFY(autoApp.tryAcquire());
}

void SingleInstanceGuardTests::secondHolderCanAcquireAfterTheFirstReleases()
{
    // The guard must not leave a sticky refusal behind: the previous instance exits (or, in
    // the QSharedMemory shape, the destructor detaches) and the next instance has to get in.
    // The restart handoff -- restart-wait-pid retrying the acquire up to 100 times -- depends
    // on this eventually-acquires property, and the elevation relaunch path depends on it too.
    const QString key = uniqueGuardKey();

    {
        SingleInstanceGuard first(key);
        QVERIFY(first.tryAcquire());
    }

    SingleInstanceGuard second(key);
    QVERIFY(second.tryAcquire());
}

QTEST_MAIN(SingleInstanceGuardTests)

#include "SingleInstanceGuardTests.moc"
