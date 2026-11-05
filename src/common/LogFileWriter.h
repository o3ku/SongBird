#pragma once

#include <functional>

#include <QDate>
#include <QDateTime>
#include <QFile>
#include <QString>
#include <QStringList>

#include "common/LogSeverity.h"

// How the on-disk log set is named, and which of its files have to go.
//
// Kept as pure functions over file names so the two rules that are easy to get
// subtly wrong -- which files belong to this writer, and which of them are the
// oldest -- can be asserted without a filesystem, a clock or a temporary
// directory. LogFileWriter is the only caller.
namespace LogFileNaming {

// A file is `<prefix>-<yyyy-MM-dd>.<NNN>.log`.
//
// The index is zero padded for one reason: it makes a plain lexicographic sort of
// the names the same order as the dates they were written in. Retention relies on
// that (it picks the oldest files by sorting), so `....13.001.log` has to sort
// after `....13.000.log` rather than before it, which an unpadded "1" would not.
constexpr int kIndexDigits = 3;

// The largest index kIndexDigits can express. A day that needs more rotations
// than this stops rotating and keeps appending to the last file: the size cap is
// best effort, and losing the records is worse than exceeding it.
constexpr int kMaxIndex = 999;

QString fileName(const QString& prefix, const QDate& date, int index);

// Every name in `existingNames` this writer produced, beyond the newest
// `keepFiles`, oldest first.
//
// Names that do not parse are ignored, and so is anything belonging to another
// prefix -- including a prefix that this one is itself a prefix of. A rule that matched on
// `startsWith` would let one log prefix collect another's files in the same directory.
QStringList filesBeyondRetention(const QStringList& existingNames, const QString& prefix, int keepFiles);

} // namespace LogFileNaming

// Appends the information panel's lines to a file, so a report can still be sent
// after the fact.
//
// The panel holds 500 lines in memory and forgets them when the process exits,
// which is what made crash-class reports unreproducible: by the time anyone asked
// for the log, the lines that explained the failure were gone. This writer is the
// other half of that -- the same lines, on disk, in a directory the user can open.
//
// Deliberately not a QObject and not a singleton. It is constructed with the
// directory it writes to, so a test points it at a temporary directory instead of
// the real application data directory, and nothing is shared between instances.
// The clock is injectable for the same reason: rotation and retention
// are decisions about dates and sizes, and a test that has to wait for midnight
// exercises nothing.
class LogFileWriter {
public:
    using Clock = std::function<QDateTime()>;

    // `keepFiles` counts every file this writer has produced, newest kept.
    // `maxFileBytes` caps a single file; when the next record would exceed it the
    // writer continues in the next index of the same day. A non-positive value for
    // either disables that rule.
    LogFileWriter(
        QString directory,
        QString filePrefix,
        int keepFiles = 3,
        qint64 maxFileBytes = 4 * 1024 * 1024,
        Clock clock = {});

    // `<QStandardPaths::AppDataLocation>/logs` -- beside `shutdown-hang.log`'s
    // directory, not in it, so retention's listing only ever sees log files. Empty
    // when the platform cannot name an application data directory, which leaves
    // the writer disabled rather than writing somewhere arbitrary.
    //
    // The application data directory rather than the executable's: a portable copy
    // can be unpacked anywhere, including a directory it may not write to, and the
    // one thing a diagnostic file must not do is fail to appear for that reason.
    static QString defaultDirectory();

    // Appends one record. The message is trimmed and an empty one is dropped.
    //
    // The caller is expected to have split multi-line text already -- AppBootstrap
    // does, which is what keeps the panel and the file agreeing on what a line is.
    // A message that still carries a newline keeps its content, and only its first
    // line gets the timestamp; the alternative, rewriting the text, would make the
    // file disagree with the core log it is meant to be checked against.
    void append(LogSeverity severity, const QString& message);

    // The file the next record goes to. Reported even while the writer is
    // disabled, so the panel can tell the user where it tried.
    QString currentFilePath() const;

    // False once a write attempt has failed -- the directory could not be created,
    // or the file could not be opened. The caller reports it once rather than per
    // line. A day boundary clears the failure, so a directory that was locked
    // today is retried tomorrow instead of for the rest of the session.
    bool isEnabled() const;

private:
    QDateTime now() const;
    void openFor(const QDate& date);
    void removeFilesBeyondRetention() const;

    QString directory_;
    QString prefix_;
    int keepFiles_ = 0;
    qint64 maxFileBytes_ = 0;
    Clock clock_;

    QFile file_;
    // The date the current file belongs to, and its index within that day. An
    // invalid date means nothing has been opened yet, which is what makes the
    // first openFor() call reset the index rather than inherit one.
    QDate openDate_;
    int index_ = 0;
    // The path currently open, or the one that was last attempted and failed.
    QString openPath_;
    QString attemptedPath_;
    QString currentPath_;
    bool enabled_ = false;
    bool retentionPending_ = true;
};
