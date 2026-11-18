#pragma once

#include <functional>

#include <QByteArray>
#include <QString>
#include <QUrl>

#include "common/OperationResult.h"
#include "runtime/core/CoreDescriptor.h"

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
    // Fetches one geodata database a core expects on disk. The file is saved under the name the
    // core reads, which is not always the name the release publishes it under.
    OperationResult updateCoreGeoFile(const CoreGeoFileRequirement& requirement) const;

private:
    void reportProgress(const QString& message) const;
    // Downloads `fileName` from `url` and saves it at `targetPath`, verifying its SHA-256
    // against the digest the releases API reports for `sourceFileName` in `repositoryPath`
    // before anything is written. A geo file with no verifiable checksum is refused, not
    // saved: a database the core trusts blindly is a routing decision made by whoever served
    // the bytes.
    OperationResult downloadAndSave(
        const QUrl& url,
        const QString& fileName,
        const QString& targetPath,
        const QString& repositoryPath,
        const QString& sourceFileName) const;
    // The SHA-256 the latest stable release of `repositoryPath` reports for `sourceFileName`,
    // or empty when the metadata cannot be fetched or names no such asset. `fromOfficial`
    // says whether the metadata arrived over the official GitHub channel: a digest from a
    // mirror only proves the file agrees with the mirror, so a match still installs, but
    // with a loud warning (see downloadAndSave).
    struct ExpectedGeoDigest {
        QString hex;
        bool fromOfficial = false;
    };
    ExpectedGeoDigest expectedDigestForGeoFile(const QString& repositoryPath, const QString& sourceFileName) const;
    static QUrl buildDownloadUrl(const QString& geoName);
    static QUrl buildSingBoxRuleSetDownloadUrl(const QString& tag);

    QString targetDirectory_;
    DownloadHandler downloadHandler_;
    ProgressHandler progressHandler_;
};
