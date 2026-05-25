#pragma once

#include <QByteArray>
#include <QString>
#include <QUrl>

#include "common/OperationResult.h"
#include "services/GeoResourceUpdateService.h"

namespace GeoResourceDownload {

OperationResult downloadBytes(
    const QUrl& url,
    const QString& fileName,
    QByteArray* content,
    const GeoResourceUpdateService::DownloadHandler& downloadHandler,
    const GeoResourceUpdateService::ProgressHandler& progressHandler);

} // namespace GeoResourceDownload
