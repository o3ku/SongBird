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
        return {false, WriteFailureStage::Open};
    }
    if (file.write(content) != content.size()) {
        return {false, WriteFailureStage::Write};
    }
    if (!file.commit()) {
        return {false, WriteFailureStage::Commit};
    }
    return {true, WriteFailureStage::None};
}

} // namespace JsonFile
