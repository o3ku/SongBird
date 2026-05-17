#pragma once

#include <functional>

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "common/OperationResult.h"
#include "domain/models/VmessItem.h"

struct CoreUpdateConfig {
    bool checkPreReleaseUpdate = false;
    bool ignoreGeoUpdateCore = false;
};

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

    struct UpdateOptions {
        ConfirmDownloadHandler confirmDownload;
        BeforeInstallHandler beforeInstall;
        ProgressHandler progressHandler;
        CancelCheckHandler cancelCheck;
        bool skipLocalVersionCheck = false;
    };

    explicit CoreUpdateService(
        DownloadHandler downloadHandler = {},
        ArchiveExtractor archiveExtractor = {},
        VersionCommandHandler versionCommandHandler = {});

    OperationResult update(
        CoreType coreType,
        const CoreUpdateConfig& config,
        const QString& targetDirectory,
        const UpdateOptions& options = {}) const;

private:
    DownloadHandler downloadHandler_;
    ArchiveExtractor archiveExtractor_;
    VersionCommandHandler versionCommandHandler_;
};
