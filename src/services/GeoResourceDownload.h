#pragma once

#include <QByteArray>
#include <QString>
#include <QUrl>

#include "common/OperationResult.h"
#include "services/GeoResourceUpdateService.h"

namespace GeoResourceDownload {

// Downloads `url`, trying the GitHub mirrors first so a download finishes where GitHub is
// unreachable, and reporting which URL actually answered in `downloadedFrom` when given.
// For release metadata the caller passes `officialFirst`: GitHub is tried first and the
// mirrors only as a fallback, because the metadata decides whether the checksum anchor
// can be trusted at all (see ReleaseChecksum.h) -- a mirror that answered first could
// hand us a digest it chose itself.
OperationResult downloadBytes(
    const QUrl& url,
    const QString& fileName,
    QByteArray* content,
    const GeoResourceUpdateService::DownloadHandler& downloadHandler,
    const GeoResourceUpdateService::ProgressHandler& progressHandler,
    bool officialFirst = false,
    QUrl* downloadedFrom = nullptr);

} // namespace GeoResourceDownload
