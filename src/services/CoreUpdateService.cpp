#include "services/CoreUpdateService.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>

#include <memory>
#include <utility>

#include "common/GitHubMirrorHelper.h"
#include "common/GitHubUrls.h"
#include "common/UserAgent.h"
#include "runtime/core/CoreBackendRegistry.h"
#include "runtime/core/ICoreBackend.h"

#if defined(Q_OS_WIN)
#include <windows.h>
#endif

namespace {

constexpr int kCoreMetadataTimeoutMs = 30000;
constexpr int kCoreDownloadTimeoutMs = 180000;
constexpr int kCancellationPollIntervalMs = 100;

struct GitHubReleaseAsset {
    QString name;
    QUrl downloadUrl;
};

struct GitHubRelease {
    QString tagName;
    bool prerelease = false;
    QList<GitHubReleaseAsset> assets;
};

struct CoreUpdateDefinition {
    const ICoreBackend* backend = nullptr;
};

GitHubRelease buildBuiltInFallbackRelease(const ICoreBackend& backend, bool prefer64Bit)
{
    const CoreUpdateAssetPolicy policy = backend.updateAssetPolicy();
    if (policy.builtInFallbackTagName.isEmpty() || policy.builtInFallbackRepositoryPath.isEmpty()) {
        return {};
    }

    const QString assetName = prefer64Bit
        ? policy.builtInFallbackAssetName64
        : policy.builtInFallbackAssetName32;
    if (assetName.isEmpty()) {
        return {};
    }

    GitHubRelease release;
    release.tagName = policy.builtInFallbackTagName;
    release.assets.append(GitHubReleaseAsset{
        assetName,
        githubReleaseDownloadUrl(policy.builtInFallbackRepositoryPath, policy.builtInFallbackTagName, assetName)});
    return release;
}

void reportProgress(const CoreUpdateService::ProgressHandler& progressHandler, const QString& message)
{
    if (progressHandler && !message.trimmed().isEmpty()) {
        progressHandler(message);
    }
}

bool isCancellationRequested(const CoreUpdateService::CancelCheckHandler& cancelCheck)
{
    return cancelCheck && cancelCheck();
}

OperationResult cancelledResult()
{
    return OperationResult::cancel(
        QCoreApplication::translate("CoreUpdateService", "Core update was canceled."));
}

bool isCancelledResult(const OperationResult& result)
{
    return result.cancelled;
}

QString normalizeVersionTag(QString value)
{
    value = value.trimmed().toLower();
    if (value.startsWith(QStringLiteral("version "))) {
        value = value.mid(QStringLiteral("version ").size()).trimmed();
    }

    if (!value.isEmpty() && value.front().isDigit()) {
        value.prepend(QChar('v'));
    }

    return value;
}

QList<int> parseVersionParts(const QString& version)
{
    QString stripped = version;
    if (stripped.startsWith(QChar('v'))) {
        stripped = stripped.mid(1);
    }
    const int dashIndex = stripped.indexOf(QChar('-'));
    if (dashIndex >= 0) {
        stripped = stripped.left(dashIndex);
    }

    QList<int> parts;
    for (const QString& segment : stripped.split(QChar('.'))) {
        bool ok = false;
        const int value = segment.toInt(&ok);
        parts.append(ok ? value : 0);
    }
    return parts;
}

bool isVersionNewerThan(const QString& candidate, const QString& current)
{
    const QList<int> candidateParts = parseVersionParts(candidate);
    const QList<int> currentParts = parseVersionParts(current);
    const int maxLen = qMax(candidateParts.size(), currentParts.size());

    for (int i = 0; i < maxLen; ++i) {
        const int c = i < candidateParts.size() ? candidateParts[i] : 0;
        const int r = i < currentParts.size() ? currentParts[i] : 0;
        if (c > r) {
            return true;
        }
        if (c < r) {
            return false;
        }
    }
    return false;
}

bool is64BitOperatingSystem()
{
#if defined(Q_OS_WIN)
    SYSTEM_INFO systemInfo{};
    GetNativeSystemInfo(&systemInfo);
    switch (systemInfo.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_AMD64:
    case PROCESSOR_ARCHITECTURE_ARM64:
        return true;
    default:
        return false;
    }
#else
    return sizeof(void*) >= 8;
#endif
}

bool containsWildcard(const QString& value)
{
    return value.contains(QChar('*')) || value.contains(QChar('?'));
}

QString findFirstExistingFile(const QString& directoryPath, const QStringList& patterns)
{
    for (const QString& pattern : patterns) {
        const QString absolutePattern = QDir(directoryPath).filePath(pattern);
        const QFileInfo info(absolutePattern);
        if (!containsWildcard(info.fileName())) {
            if (QFileInfo::exists(absolutePattern)) {
                return QDir::toNativeSeparators(QFileInfo(absolutePattern).absoluteFilePath());
            }
            continue;
        }

        QDir dir(info.path());
        if (!dir.exists()) {
            continue;
        }

        const QFileInfoList matches = dir.entryInfoList(
            QStringList{info.fileName()},
            QDir::Files | QDir::NoSymLinks | QDir::Readable,
            QDir::Name | QDir::IgnoreCase);
        if (!matches.isEmpty()) {
            return QDir::toNativeSeparators(matches.constFirst().absoluteFilePath());
        }
    }

    return {};
}

CoreUpdateDefinition resolveDefinition(CoreType coreType)
{
    return CoreUpdateDefinition{coreBackend(coreType)};
}

bool hasAnyInstalledCore(const QString& targetDirectory)
{
    for (const ICoreBackend* backend : coreBackends()) {
        if (backend == nullptr || backend->executableNames().isEmpty()) {
            continue;
        }

        if (!findFirstExistingFile(targetDirectory, backend->executableNames()).isEmpty()) {
            return true;
        }
    }

    return false;
}

bool isGeoDataFile(const QString& fileName)
{
    const QString normalized = fileName.trimmed().toLower();
    return normalized.startsWith(QStringLiteral("geo")) && normalized.endsWith(QStringLiteral(".dat"));
}

const GitHubReleaseAsset* selectBestAsset(
    const ICoreBackend& backend,
    const QList<GitHubReleaseAsset>& assets,
    bool prefer64Bit)
{
    const GitHubReleaseAsset* bestAsset = nullptr;
    int bestScore = -1;

    for (const GitHubReleaseAsset& asset : assets) {
        const int score = backend.scoreReleaseAssetName(asset.name, prefer64Bit);
        if (score > bestScore) {
            bestScore = score;
            bestAsset = &asset;
        }
    }

    return bestAsset;
}

OperationResult downloadBytesWithNetwork(
    const QUrl& url,
    QByteArray* content,
    const QString& userAgent,
    int timeoutMs,
    const CoreUpdateService::CancelCheckHandler& cancelCheck,
    const CoreUpdateService::ProgressHandler& progressHandler = {})
{
    if (content == nullptr) {
        return OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "Core download buffer is unavailable."));
    }

    if (!url.isValid() || url.scheme().trimmed().isEmpty()) {
        return OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "Core download URL is invalid."));
    }

    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, userAgent);

    bool timedOut = false;
    bool cancelled = false;
    QEventLoop loop;
    QTimer timeoutTimer;
    QTimer cancellationTimer;
    timeoutTimer.setSingleShot(true);
    cancellationTimer.setInterval(kCancellationPollIntervalMs);

    QNetworkReply* reply = manager.get(request);
    auto lastReportedPercent = std::make_shared<int>(-1);
    auto lastReportedBytes = std::make_shared<qint64>(0);
    QObject::connect(reply, &QNetworkReply::downloadProgress, reply, [progressHandler, lastReportedPercent, lastReportedBytes](qint64 bytesReceived, qint64 bytesTotal) {
        if (!progressHandler || bytesReceived < 0) {
            return;
        }

        if (bytesTotal > 0) {
            const int percent = static_cast<int>((bytesReceived * 100) / bytesTotal);
            if (percent < 100
                && *lastReportedPercent >= 0
                && percent - *lastReportedPercent < 5) {
                return;
            }

            *lastReportedPercent = percent;
            reportProgress(
                progressHandler,
                QCoreApplication::translate("CoreUpdateService", "Downloading... %1%")
                    .arg(percent));
            return;
        }

        constexpr qint64 kUnknownTotalProgressStepBytes = 512 * 1024;
        if (bytesReceived < kUnknownTotalProgressStepBytes
            || bytesReceived - *lastReportedBytes < kUnknownTotalProgressStepBytes) {
            return;
        }

        *lastReportedBytes = bytesReceived;
        reportProgress(
            progressHandler,
            QCoreApplication::translate("CoreUpdateService", "Downloading... %1 bytes")
                .arg(bytesReceived));
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        reply->abort();
        loop.quit();
    });
    QObject::connect(&cancellationTimer, &QTimer::timeout, &loop, [&]() {
        if (!isCancellationRequested(cancelCheck)) {
            return;
        }

        cancelled = true;
        reply->abort();
        loop.quit();
    });
    timeoutTimer.start(timeoutMs);
    if (cancelCheck) {
        cancellationTimer.start();
    }
    loop.exec();
    timeoutTimer.stop();
    cancellationTimer.stop();

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (cancelled) {
        reply->deleteLater();
        return cancelledResult();
    }

    if (timedOut) {
        reply->deleteLater();
        return OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "Core download timed out."));
    }

    if (reply->error() != QNetworkReply::NoError) {
        const QString errorText = reply->errorString();
        reply->deleteLater();
        return statusCode > 0
            ? OperationResult::fail(
                QCoreApplication::translate("CoreUpdateService", "HTTP %1: %2")
                    .arg(statusCode)
                    .arg(errorText))
            : OperationResult::fail(errorText);
    }

    *content = reply->readAll();
    reply->deleteLater();

    if (statusCode >= 400) {
        return OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "HTTP %1").arg(statusCode));
    }

    return OperationResult::ok();
}

OperationResult runVersionCommandWithProcess(
    const QString& program,
    const QStringList& arguments,
    QString* output,
    const CoreUpdateService::CancelCheckHandler& cancelCheck)
{
    if (output == nullptr) {
        return OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "Core version output buffer is unavailable."));
    }

    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.setWorkingDirectory(QFileInfo(program).absolutePath());
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();

    if (!process.waitForStarted(1500)) {
        return OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "Failed to start the version check process: %1")
                .arg(process.errorString()));
    }

    int elapsedMs = 0;
    while (!process.waitForFinished(kCancellationPollIntervalMs)) {
        elapsedMs += kCancellationPollIntervalMs;
        if (isCancellationRequested(cancelCheck)) {
            process.kill();
            process.waitForFinished(1500);
            return cancelledResult();
        }

        if (elapsedMs >= 5000) {
            process.kill();
            process.waitForFinished(1500);
            return OperationResult::fail(
                QCoreApplication::translate("CoreUpdateService", "The version check process timed out."));
        }
    }

    *output = QString::fromUtf8(process.readAll()).trimmed();
    return OperationResult::ok();
}

QString quotePowerShellLiteral(QString value)
{
    value.replace(QChar('\''), QStringLiteral("''"));
    return value;
}

OperationResult extractArchiveWithPowerShell(
    const QString& archivePath,
    const QString& extractionDirectory,
    const CoreUpdateService::CancelCheckHandler& cancelCheck)
{
    QDir().mkpath(extractionDirectory);

    const QString command = QStringLiteral(
                                "& { "
                                "Add-Type -AssemblyName System.IO.Compression.FileSystem; "
                                "$src = '%1'; "
                                "$dst = '%2'; "
                                "if (Test-Path -LiteralPath $dst) { Remove-Item -LiteralPath $dst -Recurse -Force; } "
                                "[System.IO.Compression.ZipFile]::ExtractToDirectory($src, $dst); "
                                " }")
                                .arg(quotePowerShellLiteral(QDir::toNativeSeparators(archivePath)))
                                .arg(quotePowerShellLiteral(QDir::toNativeSeparators(extractionDirectory)));

    QProcess process;
    process.setProgram(QStringLiteral("powershell"));
    process.setArguments(QStringList{
        QStringLiteral("-NoProfile"),
        QStringLiteral("-NonInteractive"),
        QStringLiteral("-ExecutionPolicy"),
        QStringLiteral("Bypass"),
        QStringLiteral("-Command"),
        command});
    process.start();

    if (!process.waitForStarted(1500)) {
        return OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "Failed to start PowerShell for archive extraction: %1")
                .arg(process.errorString()));
    }

    int elapsedMs = 0;
    while (!process.waitForFinished(kCancellationPollIntervalMs)) {
        elapsedMs += kCancellationPollIntervalMs;
        if (isCancellationRequested(cancelCheck)) {
            process.kill();
            process.waitForFinished(2000);
            return cancelledResult();
        }

        if (elapsedMs >= 120000) {
            process.kill();
            process.waitForFinished(2000);
            return OperationResult::fail(
                QCoreApplication::translate("CoreUpdateService", "Archive extraction timed out."));
        }
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const QString errorText = QString::fromUtf8(process.readAllStandardError()).trimmed();
        return OperationResult::fail(
            errorText.isEmpty()
                ? QCoreApplication::translate("CoreUpdateService", "Archive extraction failed.")
                : errorText);
    }

    return OperationResult::ok();
}

QString resolveExtractionRoot(const QString& extractionDirectory)
{
    QDir dir(extractionDirectory);
    if (!dir.exists()) {
        return extractionDirectory;
    }

    const QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoSymLinks | QDir::Readable);
    const QFileInfoList directories = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (files.isEmpty() && directories.size() == 1) {
        return directories.constFirst().absoluteFilePath();
    }

    return extractionDirectory;
}

OperationResult writeBytesToFile(const QString& filePath, const QByteArray& content)
{
    const QFileInfo fileInfo(filePath);
    if (!fileInfo.dir().exists() && !QDir().mkpath(fileInfo.dir().absolutePath())) {
        return OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "Failed to create the target directory for %1.")
                .arg(QDir::toNativeSeparators(filePath)));
    }

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "Failed to open %1 for writing.")
                .arg(QDir::toNativeSeparators(filePath)));
    }

    if (file.write(content) < 0) {
        return OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "Failed to write %1.")
                .arg(QDir::toNativeSeparators(filePath)));
    }

    if (!file.commit()) {
        return OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "Failed to save %1.")
                .arg(QDir::toNativeSeparators(filePath)));
    }

    return OperationResult::ok();
}

OperationResult copyExtractedFiles(
    const QString& extractionDirectory,
    const QString& targetDirectory,
    bool ignoreGeoFiles)
{
    const QString sourceRoot = resolveExtractionRoot(extractionDirectory);
    QDirIterator iterator(sourceRoot, QDir::Files | QDir::NoSymLinks | QDir::Readable, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString sourcePath = iterator.next();
        const QFileInfo sourceInfo(sourcePath);
        if (ignoreGeoFiles && isGeoDataFile(sourceInfo.fileName())) {
            continue;
        }

        QFile sourceFile(sourcePath);
        if (!sourceFile.open(QIODevice::ReadOnly)) {
            return OperationResult::fail(
                QCoreApplication::translate("CoreUpdateService", "Failed to open %1 for reading.")
                    .arg(QDir::toNativeSeparators(sourcePath)));
        }

        const QString relativePath = QDir(sourceRoot).relativeFilePath(sourcePath);
        const QString targetPath = QDir(targetDirectory).filePath(relativePath);
        const OperationResult writeResult = writeBytesToFile(targetPath, sourceFile.readAll());
        if (!writeResult.success) {
            return writeResult;
        }
    }

    return OperationResult::ok();
}

bool parseReleasePayload(
    const QByteArray& payload,
    bool allowPrerelease,
    GitHubRelease* release,
    QString* errorMessage,
    bool* stableReleaseUnavailable = nullptr)
{
    if (release == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("CoreUpdateService", "Release output buffer is unavailable.");
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (errorMessage != nullptr) {
            *errorMessage = parseError.errorString();
        }
        return false;
    }

    if (!document.isArray()) {
        if (document.isObject()) {
            const QString message = document.object().value(QStringLiteral("message")).toString().trimmed();
            if (errorMessage != nullptr) {
                *errorMessage = message.isEmpty()
                    ? QCoreApplication::translate("CoreUpdateService", "Release metadata is invalid.")
                    : message;
            }
        } else if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("CoreUpdateService", "Release metadata is invalid.");
        }
        return false;
    }

    const QJsonArray releases = document.array();
    bool skippedOnlyPrerelease = false;
    for (const QJsonValue& value : releases) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        const bool prerelease = object.value(QStringLiteral("prerelease")).toBool(false);
        if (!allowPrerelease && prerelease) {
            skippedOnlyPrerelease = true;
            continue;
        }

        GitHubRelease parsedRelease;
        parsedRelease.tagName = object.value(QStringLiteral("tag_name")).toString().trimmed();
        parsedRelease.prerelease = prerelease;
        const QJsonArray assets = object.value(QStringLiteral("assets")).toArray();
        for (const QJsonValue& assetValue : assets) {
            if (!assetValue.isObject()) {
                continue;
            }

            const QJsonObject assetObject = assetValue.toObject();
            const QUrl downloadUrl(assetObject.value(QStringLiteral("browser_download_url")).toString().trimmed());
            if (!downloadUrl.isValid()) {
                continue;
            }

            parsedRelease.assets.append(GitHubReleaseAsset{
                assetObject.value(QStringLiteral("name")).toString().trimmed(),
                downloadUrl});
        }

        if (!parsedRelease.tagName.isEmpty()) {
            *release = std::move(parsedRelease);
            return true;
        }
    }

    if (stableReleaseUnavailable != nullptr) {
        *stableReleaseUnavailable = skippedOnlyPrerelease && !allowPrerelease;
    }

    if (errorMessage != nullptr) {
        *errorMessage = skippedOnlyPrerelease && !allowPrerelease
            ? QCoreApplication::translate("CoreUpdateService", "No stable releases were found.")
            : allowPrerelease
            ? QCoreApplication::translate("CoreUpdateService", "No releases were found.")
            : QCoreApplication::translate("CoreUpdateService", "No stable releases were found.");
    }
    return false;
}

} // namespace

CoreUpdateService::CoreUpdateService(
    DownloadHandler downloadHandler,
    ArchiveExtractor archiveExtractor,
    VersionCommandHandler versionCommandHandler)
    : downloadHandler_(std::move(downloadHandler))
    , archiveExtractor_(std::move(archiveExtractor))
    , versionCommandHandler_(std::move(versionCommandHandler))
{
}

OperationResult CoreUpdateService::update(
    CoreType coreType,
    const CoreUpdateConfig& config,
    const QString& targetDirectory,
    const UpdateOptions& options) const
{
    const auto& confirmDownload = options.confirmDownload;
    const auto& beforeInstall = options.beforeInstall;
    const auto& progressHandler = options.progressHandler;
    const auto& cancelCheck = options.cancelCheck;
    const bool skipLocalVersionCheck = options.skipLocalVersionCheck;

    if (isCancellationRequested(cancelCheck)) {
        return cancelledResult();
    }

    const CoreUpdateDefinition definition = resolveDefinition(coreType);
    if (definition.backend == nullptr || !definition.backend->releasesApiUrl().isValid()) {
        return OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "Unsupported core update target: %1.")
                .arg(coreTypeDisplayName(coreType)));
    }
    const ICoreBackend& backend = *definition.backend;
    const QString displayName = backend.displayName();

    const QString normalizedTargetDirectory = targetDirectory.trimmed();
    if (normalizedTargetDirectory.isEmpty()) {
        return OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "%1 target directory is unavailable.")
                .arg(displayName));
    }

    if (!QDir().mkpath(normalizedTargetDirectory)) {
        return OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "Failed to create the target directory: %1.")
                .arg(QDir::toNativeSeparators(normalizedTargetDirectory)));
    }

    GitHubRelease release;
    QString lastError;
    const bool prefer64Bit = is64BitOperatingSystem();
    const bool noInstalledCore = !hasAnyInstalledCore(normalizedTargetDirectory);
    const GitHubRelease builtInFallbackRelease = noInstalledCore
        ? buildBuiltInFallbackRelease(backend, prefer64Bit)
        : GitHubRelease{};
    const CoreUpdateAssetPolicy assetPolicy = backend.updateAssetPolicy();
    const QString directLatestAssetName = prefer64Bit
        ? assetPolicy.directLatestAssetName64
        : assetPolicy.directLatestAssetName32;
    const QUrl directLatestDownloadUrl = prefer64Bit
        ? assetPolicy.directLatestDownloadUrl64
        : assetPolicy.directLatestDownloadUrl32;

    if (directLatestDownloadUrl.isValid() && !builtInFallbackRelease.tagName.trimmed().isEmpty()) {
        release = builtInFallbackRelease;
        reportProgress(
            progressHandler,
            QCoreApplication::translate(
                "CoreUpdateService",
                "No local core installation was found. Using built-in bootstrap %1 package: %2 (%3).")
                .arg(displayName)
                .arg(release.assets.constFirst().name)
                .arg(release.tagName));
    } else if (directLatestDownloadUrl.isValid() && !directLatestAssetName.isEmpty()) {
        release.tagName = QStringLiteral("latest");
        release.assets.append(GitHubReleaseAsset{
            directLatestAssetName,
            directLatestDownloadUrl});
        reportProgress(
            progressHandler,
            QCoreApplication::translate("CoreUpdateService", "Using direct latest %1 package: %2")
                .arg(displayName)
                .arg(directLatestAssetName));
    } else {
        reportProgress(
            progressHandler,
            QCoreApplication::translate("CoreUpdateService", "Checking the latest %1 release...")
                .arg(displayName));

        const QList<QUrl> releaseUrls = buildGitHubMirrorCandidateUrls(backend.releasesApiUrl());
        for (const QUrl& candidateUrl : releaseUrls) {
            if (isCancellationRequested(cancelCheck)) {
                return cancelledResult();
            }

            reportProgress(
                progressHandler,
                QCoreApplication::translate("CoreUpdateService", "Requesting release metadata: %1")
                    .arg(candidateUrl.toString(QUrl::FullyEncoded)));

            QByteArray payload;
            const OperationResult downloadResult = downloadHandler_
                ? downloadHandler_(candidateUrl, &payload)
                : downloadBytesWithNetwork(
                    candidateUrl,
                    &payload,
                    fallbackUserAgent(),
                    kCoreMetadataTimeoutMs,
                    cancelCheck);
            if (isCancelledResult(downloadResult)) {
                return downloadResult;
            }
            if (!downloadResult.success) {
                lastError = downloadResult.message;
                reportProgress(
                    progressHandler,
                    QCoreApplication::translate("CoreUpdateService", "Release metadata request failed: %1")
                        .arg(lastError));
                continue;
            }

            QString parseError;
            bool stableReleaseUnavailable = false;
            if (parseReleasePayload(
                    payload,
                    config.checkPreReleaseUpdate,
                    &release,
                    &parseError,
                    &stableReleaseUnavailable)) {
                lastError.clear();
                break;
            }

            lastError = parseError;
            reportProgress(
                progressHandler,
                QCoreApplication::translate("CoreUpdateService", "Release metadata parsing failed: %1")
                    .arg(lastError));
        }

        if (release.tagName.trimmed().isEmpty()) {
            if (!builtInFallbackRelease.tagName.trimmed().isEmpty()) {
                release = builtInFallbackRelease;
                reportProgress(
                    progressHandler,
                    QCoreApplication::translate(
                        "CoreUpdateService",
                        "GitHub release metadata was unavailable and no local core installation was found. Falling back to built-in %1 package: %2 (%3).")
                        .arg(displayName)
                        .arg(release.assets.constFirst().name)
                        .arg(release.tagName));
            }
        }

        if (release.tagName.trimmed().isEmpty()) {
            return OperationResult::fail(
                QCoreApplication::translate("CoreUpdateService", "Failed to resolve the latest %1 release: %2")
                    .arg(displayName)
                    .arg(lastError.isEmpty()
                        ? QCoreApplication::translate("CoreUpdateService", "Unknown error")
                        : lastError));
        }
    }

    const GitHubReleaseAsset* asset = selectBestAsset(backend, release.assets, prefer64Bit);
    if (asset == nullptr) {
        return OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "No suitable %1 asset was found in release %2.")
                .arg(displayName)
                .arg(release.tagName));
    }

    reportProgress(
        progressHandler,
        QCoreApplication::translate("CoreUpdateService", "Selected %1 package: %2")
            .arg(displayName)
            .arg(asset->name));

    if (isCancellationRequested(cancelCheck)) {
        return cancelledResult();
    }

    QString currentVersion;
    const QString executablePath = findFirstExistingFile(normalizedTargetDirectory, backend.executableNames());
    if (!skipLocalVersionCheck && !executablePath.isEmpty()) {
        reportProgress(
            progressHandler,
            QCoreApplication::translate("CoreUpdateService", "Checking the current %1 version...")
                .arg(displayName));

        QString versionOutput;
        const QStringList versionArguments = backend.versionCommandArguments();
        const OperationResult versionResult = versionCommandHandler_
            ? versionCommandHandler_(executablePath, versionArguments, &versionOutput)
            : runVersionCommandWithProcess(
                executablePath,
                versionArguments,
                &versionOutput,
                cancelCheck);
        if (isCancelledResult(versionResult)) {
            return versionResult;
        }
        if (versionResult.success) {
            currentVersion = backend.extractVersionFromOutput(versionOutput);
            if (!currentVersion.isEmpty()) {
                reportProgress(
                    progressHandler,
                    QCoreApplication::translate("CoreUpdateService", "Current %1 version: %2")
                        .arg(displayName)
                        .arg(currentVersion));
            }
        }
    }

    const QString normalizedReleaseTag = normalizeVersionTag(release.tagName);
    if (!currentVersion.isEmpty()
        && !normalizedReleaseTag.startsWith(QStringLiteral("prerelease"))
        && !isVersionNewerThan(normalizedReleaseTag, currentVersion)) {
        return OperationResult::ok(
            QCoreApplication::translate("CoreUpdateService", "%1 is already up to date: %2.")
                .arg(displayName)
                .arg(release.tagName));
    }

    if (confirmDownload) {
        const QString prompt = QCoreApplication::translate("CoreUpdateService", "Download %1 %2?\n%3")
                                   .arg(displayName)
                                   .arg(release.tagName)
                                   .arg(asset->name);
        if (!confirmDownload(prompt)) {
            return OperationResult::ok(
                QCoreApplication::translate("CoreUpdateService", "%1 update was canceled.")
                    .arg(displayName));
        }
    }

    if (isCancellationRequested(cancelCheck)) {
        return cancelledResult();
    }

    reportProgress(
        progressHandler,
        QCoreApplication::translate("CoreUpdateService", "Downloading %1 %2...")
            .arg(displayName)
            .arg(asset->name));

    QByteArray packageBytes;
    lastError.clear();
    for (const QUrl& candidateUrl : buildGitHubMirrorCandidateUrls(asset->downloadUrl)) {
        if (isCancellationRequested(cancelCheck)) {
            return cancelledResult();
        }

        reportProgress(
            progressHandler,
            QCoreApplication::translate("CoreUpdateService", "Trying download source: %1")
                .arg(candidateUrl.toString(QUrl::FullyEncoded)));

        const OperationResult downloadResult = downloadHandler_
            ? downloadHandler_(candidateUrl, &packageBytes)
            : downloadBytesWithNetwork(
                candidateUrl,
                &packageBytes,
                fallbackUserAgent(),
                kCoreDownloadTimeoutMs,
                cancelCheck,
                progressHandler);
        if (isCancelledResult(downloadResult)) {
            return downloadResult;
        }
        if (downloadResult.success && !packageBytes.isEmpty()) {
            lastError.clear();
            reportProgress(
                progressHandler,
                QCoreApplication::translate("CoreUpdateService", "Downloaded %1 (%2 bytes).")
                    .arg(asset->name)
                    .arg(packageBytes.size()));
            break;
        }

        lastError = downloadResult.success
            ? QCoreApplication::translate("CoreUpdateService", "Downloaded package is empty.")
            : downloadResult.message;
        reportProgress(
            progressHandler,
            QCoreApplication::translate("CoreUpdateService", "Download attempt failed: %1")
                .arg(lastError));
        packageBytes.clear();
    }

    if (packageBytes.isEmpty()) {
        return OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "Failed to download %1: %2")
                .arg(asset->name)
                .arg(lastError.isEmpty()
                    ? QCoreApplication::translate("CoreUpdateService", "Unknown error")
                    : lastError));
    }

    if (isCancellationRequested(cancelCheck)) {
        return cancelledResult();
    }

    if (beforeInstall) {
        reportProgress(
            progressHandler,
            QCoreApplication::translate("CoreUpdateService", "Preparing to install %1...")
                .arg(displayName));
        const OperationResult prepareResult = beforeInstall();
        if (!prepareResult.success) {
            return prepareResult;
        }
    }

    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid()) {
        return OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "Failed to create a temporary working directory."));
    }

    const QString packagePath = temporaryDirectory.filePath(asset->name);
    reportProgress(
        progressHandler,
        QCoreApplication::translate("CoreUpdateService", "Saving the update package to a temporary file..."));
    const OperationResult packageWriteResult = writeBytesToFile(packagePath, packageBytes);
    if (!packageWriteResult.success) {
        return packageWriteResult;
    }

    if (isCancellationRequested(cancelCheck)) {
        return cancelledResult();
    }

    OperationResult applyResult;
    if (asset->name.trimmed().toLower().endsWith(QStringLiteral(".zip"))) {
        const QString extractionDirectory = temporaryDirectory.filePath(QStringLiteral("extracted"));
        reportProgress(
            progressHandler,
            QCoreApplication::translate("CoreUpdateService", "Extracting %1...")
                .arg(asset->name));
        const OperationResult extractResult = archiveExtractor_
            ? archiveExtractor_(packagePath, extractionDirectory)
            : extractArchiveWithPowerShell(packagePath, extractionDirectory, cancelCheck);
        if (isCancelledResult(extractResult)) {
            return extractResult;
        }
        if (!extractResult.success) {
            return extractResult;
        }

        reportProgress(
            progressHandler,
            QCoreApplication::translate("CoreUpdateService", "Installing files to %1")
                .arg(QDir::toNativeSeparators(normalizedTargetDirectory)));
        applyResult = copyExtractedFiles(
            extractionDirectory,
            normalizedTargetDirectory,
            config.ignoreGeoUpdateCore);
    } else {
        reportProgress(
            progressHandler,
            QCoreApplication::translate("CoreUpdateService", "Installing files to %1")
                .arg(QDir::toNativeSeparators(normalizedTargetDirectory)));
        applyResult = writeBytesToFile(
            QDir(normalizedTargetDirectory).filePath(asset->name),
            packageBytes);
    }

    if (isCancellationRequested(cancelCheck)) {
        return cancelledResult();
    }

    if (!applyResult.success) {
        return applyResult;
    }

    reportProgress(
        progressHandler,
        QCoreApplication::translate("CoreUpdateService", "%1 update completed successfully.")
            .arg(displayName));

    return OperationResult::ok(
        QCoreApplication::translate("CoreUpdateService", "Updated %1 to %2 in %3.")
            .arg(displayName)
            .arg(release.tagName)
            .arg(QDir::toNativeSeparators(normalizedTargetDirectory)),
        true);
}
