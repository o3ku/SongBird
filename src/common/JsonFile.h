#pragma once

#include <QByteArray>
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
