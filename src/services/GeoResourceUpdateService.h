#pragma once

#include <functional>

#include <QByteArray>
#include <QString>
#include <QUrl>

#include "common/OperationResult.h"

class GeoResourceUpdateService {
public:
    using DownloadHandler = std::function<OperationResult(const QUrl& url, QByteArray* content)>;
    using ProgressHandler = std::function<void(const QString& message)>;

    explicit GeoResourceUpdateService(
        QString targetDirectory = {},
        DownloadHandler downloadHandler = {},
        ProgressHandler progressHandler = {});

    OperationResult update(const QString& geoName) const;
    OperationResult updateSingBoxRuleSet(const QString& tag) const;

private:
    void reportProgress(const QString& message) const;
    OperationResult downloadAndSave(
        const QUrl& url,
        const QString& fileName,
        const QString& targetPath) const;
    OperationResult downloadBytes(const QUrl& url, const QString& fileName, QByteArray* content) const;
    static QUrl buildDownloadUrl(const QString& geoName);
    static QUrl buildSingBoxRuleSetDownloadUrl(const QString& tag);

    QString targetDirectory_;
    DownloadHandler downloadHandler_;
    ProgressHandler progressHandler_;
};
