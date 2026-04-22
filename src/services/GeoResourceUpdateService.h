#pragma once

#include <functional>

#include <QByteArray>
#include <QString>
#include <QUrl>

#include "common/OperationResult.h"

class GeoResourceUpdateService {
public:
    using DownloadHandler = std::function<OperationResult(const QUrl& url, QByteArray* content)>;

    explicit GeoResourceUpdateService(QString targetDirectory = {}, DownloadHandler downloadHandler = {});

    OperationResult update(const QString& geoName) const;

private:
    OperationResult downloadBytes(const QUrl& url, QByteArray* content) const;
    static QUrl buildDownloadUrl(const QString& geoName);

    QString targetDirectory_;
    DownloadHandler downloadHandler_;
};
