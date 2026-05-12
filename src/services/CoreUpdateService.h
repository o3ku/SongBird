#pragma once

#include <functional>

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "common/OperationResult.h"
#include "domain/models/Config.h"

class CoreUpdateService {
public:
    using DownloadHandler = std::function<OperationResult(const QUrl& url, QByteArray* content)>;
    using ArchiveExtractor = std::function<OperationResult(const QString& archivePath, const QString& extractionDirectory)>;
    using ConfirmDownloadHandler = std::function<bool(const QString& prompt)>;
    using BeforeInstallHandler = std::function<OperationResult()>;
    using ProgressHandler = std::function<void(const QString& message)>;
    using CancelCheckHandler = std::function<bool()>;
    using VersionCommandHandler =
        std::function<OperationResult(const QString& program, const QStringList& arguments, QString* output)>;

    explicit CoreUpdateService(
        DownloadHandler downloadHandler = {},
        ArchiveExtractor archiveExtractor = {},
        VersionCommandHandler versionCommandHandler = {});

    OperationResult update(
        CoreType coreType,
        const Config& config,
        const QString& targetDirectory,
        ConfirmDownloadHandler confirmDownload = {},
        BeforeInstallHandler beforeInstall = {},
        ProgressHandler progressHandler = {},
        CancelCheckHandler cancelCheck = {},
        bool skipLocalVersionCheck = false) const;

private:
    DownloadHandler downloadHandler_;
    ArchiveExtractor archiveExtractor_;
    VersionCommandHandler versionCommandHandler_;
};
