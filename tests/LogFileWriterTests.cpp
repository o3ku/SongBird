#include <QtTest>

#include <functional>
#include <memory>

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QTemporaryDir>
#include <QTime>

#include "common/LogFileWriter.h"

namespace {

// A clock the test moves by hand, shared with the writer it is handed to.
//
// The writer takes its clock by value, so a plain functor would be copied and the
// test's later calls would move a date the writer never looks at. Going through a
// shared date is what makes "advance a day, write again" mean anything.
class TestClock {
public:
    explicit TestClock(const QDateTime& start)
        : moment_(std::make_shared<QDateTime>(start))
    {
    }

    void advanceDays(int days)
    {
        *moment_ = moment_->addDays(days);
    }

    std::function<QDateTime()> reader() const
    {
        const std::shared_ptr<QDateTime> moment = moment_;
        return [moment]() { return *moment; };
    }

private:
    std::shared_ptr<QDateTime> moment_;
};

void writeFile(const QString& path, const QString& contents)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text), qPrintable(path));
    file.write(contents.toUtf8());
    file.close();
}

// Read back through the same text mode the writer wrote with, so the comparison
// below is about the record and not about which platform translates line endings.
QString readAll(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

QStringList directoryNames(const QString& directory)
{
    QStringList names = QDir(directory).entryList(QDir::Files);
    names.sort();
    return names;
}

QString pathIn(const QString& directory, const QString& name)
{
    return QDir(directory).filePath(name);
}

} // namespace

class LogFileWriterTests : public QObject {
    Q_OBJECT

private slots:
    void writesTimestampSeverityAndMessageOnOneLine();
    void skipsEmptyAndWhitespaceOnlyMessages();
    void multiLineMessageOnlyTimestampsItsFirstLine();
    void startsANewFileWhenTheDateChanges();
    void rotatesWithinTheDayWhenTheSizeCapIsReached();
    void retentionKeepsOnlyTheNewestFiles();
    void retentionLeavesNamesItDidNotWriteAlone();
    void retentionIgnoresAForeignPrefixThatStartsWithItsOwn();
    void fileNamePaddingMakesStringOrderChronological();
    void filesBeyondRetentionPicksTheOldestBySortedName();
    void staysDisabledWhenTheDirectoryCannotBeCreated();
    void writesNothingWhenNoDirectoryIsConfigured();
    void defaultDirectoryIsALogsSubdirectory();
};

void LogFileWriterTests::writesTimestampSeverityAndMessageOnOneLine()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    TestClock clock(QDateTime(QDate(2026, 10, 13), QTime(10, 12, 22)));
    LogFileWriter writer(directory.path(), QStringLiteral("songbird"), 3, 4 * 1024 * 1024, clock.reader());

    // Opened by the constructor, not by the first append: the panel's opening
    // lines name the file, so the path has to be answerable before anything is
    // logged.
    QVERIFY(writer.isEnabled());
    QCOMPARE(
        QFileInfo(writer.currentFilePath()).fileName(),
        QStringLiteral("songbird-2026-10-13.000.log"));

    writer.append(LogSeverity::Info, QStringLiteral("SongBird 2.5.0-beta3 | Qt 5.15.2 | windows x86_64"));
    writer.append(LogSeverity::Warning, QStringLiteral("something to look at"));
    writer.append(LogSeverity::Error, QStringLiteral("activation failed"));

    QCOMPARE(
        readAll(writer.currentFilePath()),
        QStringLiteral("2026-10-13T10:12:22 [I] SongBird 2.5.0-beta3 | Qt 5.15.2 | windows x86_64\n"
                       "2026-10-13T10:12:22 [W] something to look at\n"
                       "2026-10-13T10:12:22 [E] activation failed\n"));
}

void LogFileWriterTests::skipsEmptyAndWhitespaceOnlyMessages()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    TestClock clock(QDateTime(QDate(2026, 10, 13), QTime(10, 12, 22)));
    LogFileWriter writer(directory.path(), QStringLiteral("songbird"), 3, 0, clock.reader());

    writer.append(LogSeverity::Info, QString());
    writer.append(LogSeverity::Info, QStringLiteral("   "));
    writer.append(LogSeverity::Info, QStringLiteral("\n"));
    writer.append(LogSeverity::Info, QStringLiteral("  kept  "));

    // Trailing whitespace goes too: the panel trims what it shows, and a file that
    // kept it would disagree with the panel about the same line.
    QCOMPARE(readAll(writer.currentFilePath()), QStringLiteral("2026-10-13T10:12:22 [I] kept\n"));
}

void LogFileWriterTests::multiLineMessageOnlyTimestampsItsFirstLine()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    TestClock clock(QDateTime(QDate(2026, 10, 13), QTime(10, 12, 22)));
    LogFileWriter writer(directory.path(), QStringLiteral("songbird"), 3, 0, clock.reader());

    // The writer does not split; AppBootstrap does, which is what keeps the panel
    // and the file agreeing on what a line is. This pins that contract rather than
    // leaving it to be discovered: a message that still carries a newline keeps its
    // content, and only the first line is timestamped.
    writer.append(LogSeverity::Info, QStringLiteral("first\nsecond"));

    QCOMPARE(readAll(writer.currentFilePath()), QStringLiteral("2026-10-13T10:12:22 [I] first\nsecond\n"));
}

void LogFileWriterTests::startsANewFileWhenTheDateChanges()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    TestClock clock(QDateTime(QDate(2026, 10, 13), QTime(23, 59, 59)));
    LogFileWriter writer(directory.path(), QStringLiteral("songbird"), 3, 0, clock.reader());
    writer.append(LogSeverity::Info, QStringLiteral("day one"));

    const QString firstDay = pathIn(directory.path(), QStringLiteral("songbird-2026-10-13.000.log"));
    QVERIFY(QFileInfo::exists(firstDay));

    clock.advanceDays(1);
    writer.append(LogSeverity::Info, QStringLiteral("day two"));

    QCOMPARE(
        QFileInfo(writer.currentFilePath()).fileName(),
        QStringLiteral("songbird-2026-10-14.000.log"));
    QCOMPARE(
        readAll(firstDay),
        QStringLiteral("2026-10-13T23:59:59 [I] day one\n"));
    QCOMPARE(
        readAll(pathIn(directory.path(), QStringLiteral("songbird-2026-10-14.000.log"))),
        QStringLiteral("2026-10-14T23:59:59 [I] day two\n"));
}

void LogFileWriterTests::rotatesWithinTheDayWhenTheSizeCapIsReached()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    TestClock clock(QDateTime(QDate(2026, 10, 13), QTime(10, 12, 22)));
    // One record is 31 bytes ("2026-10-13T10:12:22 [I] record\n"), so a 40 byte cap
    // fits exactly one of them and the second has to roll over. A cap that fits two
    // would not distinguish "rotated" from "not rotated yet".
    LogFileWriter writer(directory.path(), QStringLiteral("songbird"), 3, 40, clock.reader());

    writer.append(LogSeverity::Info, QStringLiteral("record"));
    writer.append(LogSeverity::Info, QStringLiteral("record"));

    QCOMPARE(
        directoryNames(directory.path()),
        QStringList({QStringLiteral("songbird-2026-10-13.000.log"),
                     QStringLiteral("songbird-2026-10-13.001.log")}));
    QCOMPARE(
        readAll(pathIn(directory.path(), QStringLiteral("songbird-2026-10-13.000.log"))),
        QStringLiteral("2026-10-13T10:12:22 [I] record\n"));
    QCOMPARE(
        readAll(pathIn(directory.path(), QStringLiteral("songbird-2026-10-13.001.log"))),
        QStringLiteral("2026-10-13T10:12:22 [I] record\n"));
}

void LogFileWriterTests::retentionKeepsOnlyTheNewestFiles()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    TestClock clock(QDateTime(QDate(2026, 10, 1), QTime(9, 0, 0)));
    LogFileWriter writer(directory.path(), QStringLiteral("songbird"), 3, 0, clock.reader());

    for (int day = 1; day <= 5; ++day) {
        writer.append(LogSeverity::Info, QStringLiteral("day %1").arg(day));
        clock.advanceDays(1);
    }

    // Retention runs when a file is opened, so the fifth day's open is what trims
    // the set down to the three it is allowed to keep.
    QCOMPARE(
        directoryNames(directory.path()),
        QStringList({QStringLiteral("songbird-2026-10-03.000.log"),
                     QStringLiteral("songbird-2026-10-04.000.log"),
                     QStringLiteral("songbird-2026-10-05.000.log")}));
}

void LogFileWriterTests::retentionLeavesNamesItDidNotWriteAlone()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    writeFile(pathIn(directory.path(), QStringLiteral("songbird-2026-10-01.000.log")), QStringLiteral("old\n"));
    writeFile(pathIn(directory.path(), QStringLiteral("songbird-2026-10-02.000.log")), QStringLiteral("old\n"));
    writeFile(pathIn(directory.path(), QStringLiteral("songbird-2026-10-03.000.log")), QStringLiteral("old\n"));
    // All three of these sit in the same directory and none of them is this
    // writer's to delete: another component's diagnostic file, a name the user
    // created, and a name that does not carry the index this writer always writes.
    writeFile(pathIn(directory.path(), QStringLiteral("shutdown-hang.log")), QStringLiteral("other\n"));
    writeFile(pathIn(directory.path(), QStringLiteral("songbird.log")), QStringLiteral("user\n"));
    writeFile(pathIn(directory.path(), QStringLiteral("songbird-2026-10-01.log")), QStringLiteral("user\n"));

    TestClock clock(QDateTime(QDate(2026, 10, 10), QTime(9, 0, 0)));
    LogFileWriter writer(directory.path(), QStringLiteral("songbird"), 1, 0, clock.reader());

    QCOMPARE(
        directoryNames(directory.path()),
        QStringList({QStringLiteral("shutdown-hang.log"),
                     QStringLiteral("songbird-2026-10-01.log"),
                     QStringLiteral("songbird-2026-10-10.000.log"),
                     QStringLiteral("songbird.log")}));
}

void LogFileWriterTests::retentionIgnoresAForeignPrefixThatStartsWithItsOwn()
{
    // The rule first, on names alone, and this is the assertion that actually pins
    // it. The filesystem half below cannot see a loosened pattern: with one, the
    // file that gets evicted is today's own -- and today's own is the one the
    // writer still holds open, which Windows refuses to delete while POSIX happily
    // unlinks it. So the same mistake leaves the directory looking correct on
    // Windows and deletes the current log on Linux and macOS.
    QCOMPARE(
        LogFileNaming::filesBeyondRetention(
            QStringList({QStringLiteral("songbird-auto-2026-10-01.000.log"),
                         QStringLiteral("songbird-2026-10-01.000.log"),
                         QStringLiteral("songbird-2026-10-10.000.log")}),
            QStringLiteral("songbird"),
            1),
        QStringList({QStringLiteral("songbird-2026-10-01.000.log")}));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    // Logs live in the same directory, and "songbird" is a
    // prefix of "songbird-auto". A rule that matched on startsWith() would have
    // each of them delete the other's history -- and only when a user happened to
    // run both, which is the sort of thing no amount of manual testing finds.
    writeFile(pathIn(directory.path(), QStringLiteral("songbird-auto-2026-10-01.000.log")), QStringLiteral("auto\n"));
    writeFile(pathIn(directory.path(), QStringLiteral("songbird-2026-10-01.000.log")), QStringLiteral("mine\n"));

    TestClock clock(QDateTime(QDate(2026, 10, 10), QTime(9, 0, 0)));
    LogFileWriter mainWriter(directory.path(), QStringLiteral("songbird"), 1, 0, clock.reader());

    QCOMPARE(
        directoryNames(directory.path()),
        QStringList({QStringLiteral("songbird-2026-10-10.000.log"),
                     QStringLiteral("songbird-auto-2026-10-01.000.log")}));

    // And the mirror image, which is the half that would have gone unnoticed: the
    // longer prefix must not reach back into the shorter one either.
    writeFile(pathIn(directory.path(), QStringLiteral("songbird-2026-10-02.000.log")), QStringLiteral("mine\n"));
    LogFileWriter autoWriter(directory.path(), QStringLiteral("songbird-auto"), 1, 0, clock.reader());

    QCOMPARE(
        directoryNames(directory.path()),
        QStringList({QStringLiteral("songbird-2026-10-02.000.log"),
                     QStringLiteral("songbird-2026-10-10.000.log"),
                     QStringLiteral("songbird-auto-2026-10-10.000.log")}));
}

void LogFileWriterTests::fileNamePaddingMakesStringOrderChronological()
{
    QCOMPARE(
        LogFileNaming::fileName(QStringLiteral("songbird"), QDate(2026, 10, 13), 0),
        QStringLiteral("songbird-2026-10-13.000.log"));

    // Retention sorts names as strings, so these two comparisons are the invariant
    // it rests on. Without the padding "1" would sort after "10", and retention
    // would delete the newest file of the day and keep the oldest.
    QVERIFY(
        LogFileNaming::fileName(QStringLiteral("songbird"), QDate(2026, 10, 13), 1)
        < LogFileNaming::fileName(QStringLiteral("songbird"), QDate(2026, 10, 13), 10));
    QVERIFY(
        LogFileNaming::fileName(QStringLiteral("songbird"), QDate(2026, 10, 9), 999)
        < LogFileNaming::fileName(QStringLiteral("songbird"), QDate(2026, 10, 10), 0));
}

void LogFileWriterTests::filesBeyondRetentionPicksTheOldestBySortedName()
{
    const QStringList names({QStringLiteral("songbird-2026-10-03.000.log"),
                             QStringLiteral("songbird-2026-10-01.000.log"),
                             QStringLiteral("songbird-2026-10-02.000.log"),
                             QStringLiteral("shutdown-hang.log")});

    QCOMPARE(
        LogFileNaming::filesBeyondRetention(names, QStringLiteral("songbird"), 2),
        QStringList({QStringLiteral("songbird-2026-10-01.000.log")}));
    QCOMPARE(
        LogFileNaming::filesBeyondRetention(names, QStringLiteral("songbird"), 1),
        QStringList({QStringLiteral("songbird-2026-10-01.000.log"),
                     QStringLiteral("songbird-2026-10-02.000.log")}));

    // Nothing to do, and a non-positive count means "keep everything" rather than
    // "delete everything".
    QCOMPARE(LogFileNaming::filesBeyondRetention(names, QStringLiteral("songbird"), 3), QStringList());
    QCOMPARE(LogFileNaming::filesBeyondRetention(names, QStringLiteral("songbird"), 0), QStringList());
    QCOMPARE(LogFileNaming::filesBeyondRetention(QStringList(), QStringLiteral("songbird"), 1), QStringList());
}

void LogFileWriterTests::staysDisabledWhenTheDirectoryCannotBeCreated()
{
    QTemporaryDir parent;
    QVERIFY(parent.isValid());

    // A file where the directory has to be: mkpath() cannot succeed, and this is
    // the cheap stand-in for the real cases (a read-only installation directory, a
    // policy that denies writes) that a test cannot arrange portably.
    const QString blocker = pathIn(parent.path(), QStringLiteral("blocked"));
    writeFile(blocker, QStringLiteral("not a directory\n"));

    TestClock clock(QDateTime(QDate(2026, 10, 13), QTime(10, 12, 22)));
    LogFileWriter writer(
        pathIn(blocker, QStringLiteral("logs")), QStringLiteral("songbird"), 3, 0, clock.reader());

    QVERIFY(!writer.isEnabled());
    // Still answerable, because the panel says where it tried -- and that is the
    // only way the user can tell "the log is empty" from "the log is elsewhere".
    QCOMPARE(
        QFileInfo(writer.currentFilePath()).fileName(),
        QStringLiteral("songbird-2026-10-13.000.log"));

    writer.append(LogSeverity::Error, QStringLiteral("still not written"));
    QVERIFY(!writer.isEnabled());
    QVERIFY(!QFileInfo::exists(pathIn(blocker, QStringLiteral("logs"))));
}

void LogFileWriterTests::writesNothingWhenNoDirectoryIsConfigured()
{
    // What a window constructed without a writer gets: no path, no file, no
    // failure. This is the state the widget tests run in.
    LogFileWriter writer(QString(), QStringLiteral("songbird"));

    QVERIFY(!writer.isEnabled());
    QVERIFY(writer.currentFilePath().isEmpty());

    writer.append(LogSeverity::Info, QStringLiteral("nowhere to go"));
    QVERIFY(!writer.isEnabled());
    QVERIFY(writer.currentFilePath().isEmpty());
}

void LogFileWriterTests::defaultDirectoryIsALogsSubdirectory()
{
    const QString directory = LogFileWriter::defaultDirectory();
    if (directory.isEmpty()) {
        // QStandardPaths is allowed to answer "nothing" on a platform that has no
        // application data directory; the writer then stays disabled, which the
        // case above covers.
        QSKIP("this platform reports no application data directory");
    }

    // A subdirectory, not the application data directory itself: that one already
    // holds shutdown-hang.log, and retention's listing must only ever see files it
    // is allowed to delete.
    QCOMPARE(QDir(directory).dirName(), QStringLiteral("logs"));
}

QTEST_MAIN(LogFileWriterTests)
#include "LogFileWriterTests.moc"
