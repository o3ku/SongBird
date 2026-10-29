#pragma once

#include <QByteArray>
#include <QFile>
#include <QIODevice>
#include <QSaveFile>
#include <QString>

namespace JsonFile {

enum class WriteFailureStage {
    None,
    Open,
    Write,
    Commit,
};

struct WriteResult {
    bool ok = false;
    WriteFailureStage stage = WriteFailureStage::None;
    // What the OS said went wrong, when it said anything ("Permission denied", "No such file or
    // directory"). Empty on success, and empty when the failure produced no message of its own --
    // so treat it as extra detail, never as the whole reason.
    QString errorString;
};

// Atomically writes `content` to `path` via QSaveFile. Callers own the
// user-facing error text; `stage` reports which step failed so messages can
// stay specific (open vs write vs commit).
inline WriteResult writeFileAtomically(
    const QString& path,
    const QByteArray& content,
    QIODevice::OpenModeFlag textFlag = QIODevice::NotOpen)
{
    // An identical rewrite is skipped rather than replaced: QSaveFile's temp-and-replace gives
    // the file a brand-new identity, so a cloud-synced folder (OneDrive) treats every save as a
    // modification to copy -- a state-only save was re-uploading the unchanged primary config on
    // every probe round, and two machines doing that could only end in conflict copies. Reading
    // the existing file with the same text flag compares what a write would produce (line-ending
    // translation included) against what is already on disk. The reader is scoped: a handle left
    // open on Windows blocks the replace below.
    bool identicalOnDisk = false;
    {
        QFile existing(path);
        if (existing.open(QIODevice::ReadOnly | textFlag)) {
            identicalOnDisk = existing.readAll() == content;
        }
    }
    if (identicalOnDisk) {
        return {true, WriteFailureStage::None, {}};
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | textFlag)) {
        return {false, WriteFailureStage::Open, file.errorString()};
    }
    if (file.write(content) != content.size()) {
        return {false, WriteFailureStage::Write, file.errorString()};
    }
    if (!file.commit()) {
        return {false, WriteFailureStage::Commit, file.errorString()};
    }
    return {true, WriteFailureStage::None, {}};
}

} // namespace JsonFile
