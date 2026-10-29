#include "appcore/StartupResourceBroker.h"

#include <utility>

#include <QDir>
#include <QMetaObject>
#include <QSet>
#include <QThread>

#include "common/AppPaths.h"
#include "common/BackgroundThreadLaunch.h"
#include "common/SingBoxPaths.h"
#include "domain/models/VmessItem.h"
#include "services/CoreUpdateService.h"
#include "services/GeoResourceUpdateService.h"

// Translation context: every sentence this broker emits already exists in the .ts under the
// "ProxySession" context. Rather than duplicating tr() calls here under a new context -- which
// would orphan the existing entries and need a retranslation -- the session hands over the
// finished sentences as providers (SessionStrings, taken by the constructor), exactly as
// ProxyHealthWatchdog did. This keeps the split translation-neutral: the .ts and the compiled
// .qm are untouched by it.

namespace {

// Moved from ProxySession.cpp's anonymous namespace: only the download-and-resume paths used
// it. Normalize + de-duplicate rule-set tags in one place, because both halves of the pair
// (the missing-tags scan and this downloader) agree only if they normalize identically.
QStringList normalizedUniqueTags(const QStringList& tags)
{
    QStringList normalizedTags;
    QSet<QString> seenTags;
    for (const QString& tag : tags) {
        const QString normalizedTag = tag.trimmed().toLower();
        if (normalizedTag.isEmpty() || seenTags.contains(normalizedTag)) {
            continue;
        }
        seenTags.insert(normalizedTag);
        normalizedTags.append(normalizedTag);
    }
    return normalizedTags;
}

// Moved from ProxySession.cpp's anonymous namespace: folds per-file download results into the
// one OperationResult the shared finish handler decides on. Joined messages, because the user
// sees one line per step.
OperationResult combineOperationResults(const QList<OperationResult>& results)
{
    bool success = true;
    QStringList messages;
    for (const OperationResult& result : results) {
        success = success && result.success;
        if (!result.message.trimmed().isEmpty()) {
            messages.append(result.message.trimmed());
        }
    }

    const QString message = messages.join(QChar('\n'));
    return success ? OperationResult::ok(message) : OperationResult::fail(message);
}

} // namespace

StartupResourceBroker::StartupResourceBroker(
    std::weak_ptr<char> sessionLifetimeGuard,
    Dependencies deps,
    SessionStrings sessionStrings,
    SessionHooks hooks,
    QObject* parent)
    : QObject(parent)
    , sessionLifetimeGuard_(std::move(sessionLifetimeGuard))
    , deps_(deps)
    , sessionStrings_(std::move(sessionStrings))
    , hooks_(std::move(hooks))
{
}

void StartupResourceBroker::downloadMissingGeoFilesAndResume(
    const CoreInfo& coreInfo,
    const QString& geoValidationMessage,
    const QString& checkGeoStep,
    bool skipTunCleanup,
    bool showStartupOverlay)
{
    const QString targetDirectory = coreGeoDirectory(coreInfo);
    if (targetDirectory.trimmed().isEmpty()) {
        hooks_.failStartup(OperationResult::fail(sessionStrings_.coreDataDirectoryEmptySentence()));
        return;
    }

    const QList<CoreGeoFileRequirement> requirements = coreGeoFileRequirementsFor(coreInfo);
    if (requirements.isEmpty()) {
        // Nothing to fetch after all; go straight on rather than stalling on an empty download.
        hooks_.resumeStartup(skipTunCleanup, showStartupOverlay);
        return;
    }

    const QString startMessage = sessionStrings_.geoDownloadStartSentence(
        QDir::toNativeSeparators(targetDirectory));
    hooks_.setCheckpointStatus(CoreStartupCheckpointStatus::Started, checkGeoStep, startMessage);
    hooks_.logMessage(geoValidationMessage);

    hooks_.setPhase(ProxySession::Phase::ValidateRuntimeResources);

    const std::weak_ptr<char> guard = sessionLifetimeGuard_;
    QThread* thread = launchBackgroundThread([this, targetDirectory, requirements, checkGeoStep,
                                       skipTunCleanup, showStartupOverlay, guard]() {
        const auto reportProgress = [this, checkGeoStep, guard](const QString& message) {
            postStartupDownloadProgress(checkGeoStep, message, guard);
        };

        GeoResourceUpdateService geoUpdateService(targetDirectory, {}, reportProgress);
        QList<OperationResult> results;
        for (const CoreGeoFileRequirement& requirement : requirements) {
            // A file already on disk is not fetched again, so a retry after a canceled or failed
            // download resumes instead of re-pulling everything the core needs.
            if (coreGeoFileIsPresent(targetDirectory, requirement.fileName)) {
                continue;
            }
            results.append(geoUpdateService.updateCoreGeoFile(requirement));
        }
        const OperationResult result = combineOperationResults(results);

        QMetaObject::invokeMethod(this, [this, result, checkGeoStep, skipTunCleanup,
                                         showStartupOverlay, guard]() {
            if (guard.expired() || hooks_.shuttingDown()) {
                return;
            }
            handleStartupDownloadFinished(
                checkGeoStep,
                result,
                skipTunCleanup,
                showStartupOverlay,
                sessionStrings_.geoCanceledSentence(),
                sessionStrings_.geoCompletedButCanceledSentence());
        }, Qt::QueuedConnection);
    }, [this](QThread* worker) { hooks_.trackThread(worker); });
    thread->start();
}

void StartupResourceBroker::downloadMissingSingBoxRuleSetsAndResume(
    const QStringList& missingRuleSetTags,
    const QString& checkGeoStep,
    bool skipTunCleanup,
    bool showStartupOverlay)
{
    const QStringList tags = normalizedUniqueTags(missingRuleSetTags);
    if (tags.isEmpty()) {
        hooks_.resumeStartup(skipTunCleanup, showStartupOverlay);
        return;
    }

    const QString targetDirectory = AppPaths::applicationDirectory();
    const QString startMessage = sessionStrings_.ruleSetDownloadStartSentence(
        tags.size(),
        QDir::toNativeSeparators(QDir(targetDirectory).filePath(SingBoxPaths::ruleSetDirectoryName())));
    hooks_.setCheckpointStatus(CoreStartupCheckpointStatus::Started, checkGeoStep, startMessage);

    hooks_.setPhase(ProxySession::Phase::ValidateRuntimeResources);

    const std::weak_ptr<char> guard = sessionLifetimeGuard_;
    QThread* thread = launchBackgroundThread([this, targetDirectory, tags, checkGeoStep, skipTunCleanup,
                                       showStartupOverlay, guard]() {
        const auto reportProgress = [this, checkGeoStep, guard](const QString& message) {
            postStartupDownloadProgress(checkGeoStep, message, guard);
        };

        GeoResourceUpdateService geoUpdateService(targetDirectory, {}, reportProgress);
        QList<OperationResult> results;
        for (const QString& tag : tags) {
            results.append(geoUpdateService.updateSingBoxRuleSet(tag));
        }
        const OperationResult result = combineOperationResults(results);

        QMetaObject::invokeMethod(this, [this, result, checkGeoStep, skipTunCleanup,
                                         showStartupOverlay, guard]() {
            if (guard.expired() || hooks_.shuttingDown()) {
                return;
            }
            handleStartupDownloadFinished(
                checkGeoStep,
                result,
                skipTunCleanup,
                showStartupOverlay,
                sessionStrings_.ruleSetCanceledSentence(),
                sessionStrings_.ruleSetCompletedButCanceledSentence());
        }, Qt::QueuedConnection);
    }, [this](QThread* worker) { hooks_.trackThread(worker); });
    thread->start();
}

void StartupResourceBroker::downloadMissingCoreAndResume(
    CoreType coreType,
    const OperationResult& missingCoreResult,
    const QString& downloadCoreStep,
    bool skipTunCleanup,
    bool showStartupOverlay,
    const CoreUpdateConfig& updateConfig)
{
    const CoreType runtimeCore = deps_.profileResolver.resolveRuntimeCoreType(coreType);
    const QString installDirectory = deps_.profileResolver.resolveCoreInstallDirectory(runtimeCore);
    if (runtimeCore == CoreType::Unknown || installDirectory.trimmed().isEmpty()) {
        hooks_.setCheckpointStatus(CoreStartupCheckpointStatus::Failed, downloadCoreStep,
            missingCoreResult.message);
        hooks_.failStartup(missingCoreResult);
        return;
    }

    const QString startMessage = sessionStrings_.coreDownloadStartSentence(
        coreTypeDisplayName(runtimeCore),
        QDir::toNativeSeparators(installDirectory));
    hooks_.setCheckpointStatus(CoreStartupCheckpointStatus::Started, downloadCoreStep, startMessage);
    hooks_.logMessage(missingCoreResult.message);

    const BackgroundTaskCoordinator::Token token =
        deps_.backgroundTasks.tryBeginStartupTask(BackgroundTaskCoordinator::Kind::CoreUpdate);
    if (!token.isValid()) {
        hooks_.setCheckpointStatus(CoreStartupCheckpointStatus::Failed, downloadCoreStep,
            sessionStrings_.backgroundTaskBusySentence());
        hooks_.failStartup(OperationResult::fail(
            sessionStrings_.coreDownloadBusyFailSentence(coreTypeDisplayName(runtimeCore))));
        return;
    }

    hooks_.setPhase(ProxySession::Phase::ValidateCoreApplication);

    const std::weak_ptr<char> guard = sessionLifetimeGuard_;

    QThread* thread = launchBackgroundThread([this, runtimeCore, updateConfig, installDirectory,
                                       downloadCoreStep, skipTunCleanup,
                                       showStartupOverlay, token, guard]() {
        CoreUpdateService coreUpdateService;
        CoreUpdateService::UpdateOptions options;
        options.progressHandler = [this, downloadCoreStep, token, guard](const QString& message) {
            postStartupDownloadProgress(downloadCoreStep, message, token, guard);
        };
        options.cancelCheck = [this]() {
            QThread* currentThread = QThread::currentThread();
            return hooks_.shuttingDown()
                || (currentThread != nullptr && currentThread->isInterruptionRequested());
        };
        options.skipLocalVersionCheck = true;

        const OperationResult result = coreUpdateService.update(
            runtimeCore, updateConfig, installDirectory, options);

        QMetaObject::invokeMethod(this, [this, downloadCoreStep, skipTunCleanup,
                                         showStartupOverlay, token, result, guard]() {
            if (guard.expired()) {
                return;
            }
            if (!deps_.backgroundTasks.isCurrent(token)) {
                return;
            }
            deps_.backgroundTasks.finish(token);
            if (hooks_.shuttingDown()) {
                return;
            }
            handleStartupDownloadFinished(
                downloadCoreStep,
                result,
                skipTunCleanup,
                showStartupOverlay,
                sessionStrings_.coreCanceledSentence(),
                sessionStrings_.coreCompletedButCanceledSentence(),
                [this]() { deps_.activationCoordinator.refreshExistingCoreTypes(); });
        }, Qt::QueuedConnection);
    }, [this](QThread* worker) { hooks_.trackThread(worker); });
    thread->start();
}

void StartupResourceBroker::postStartupDownloadProgress(
    const QString& step,
    const QString& message,
    const std::weak_ptr<char>& guard)
{
    if (message.trimmed().isEmpty()) {
        return;
    }

    QMetaObject::invokeMethod(this, [this, step, message, guard]() {
        if (guard.expired() || !hooks_.isActivationInProgress()) {
            return;
        }
        hooks_.setCheckpointStatus(CoreStartupCheckpointStatus::Started, step, message);
    }, Qt::QueuedConnection);
}

void StartupResourceBroker::postStartupDownloadProgress(
    const QString& step,
    const QString& message,
    const BackgroundTaskCoordinator::Token& token,
    const std::weak_ptr<char>& guard)
{
    if (message.trimmed().isEmpty()) {
        return;
    }

    QMetaObject::invokeMethod(this, [this, step, message, token, guard]() {
        if (guard.expired()) {
            return;
        }
        if (!deps_.backgroundTasks.isCurrent(token)) {
            return;
        }
        if (!hooks_.isActivationInProgress()) {
            return;
        }
        hooks_.setCheckpointStatus(CoreStartupCheckpointStatus::Started, step, message);
    }, Qt::QueuedConnection);
}

void StartupResourceBroker::handleStartupDownloadFinished(
    const QString& step,
    const OperationResult& result,
    bool skipTunCleanup,
    bool showStartupOverlay,
    const QString& canceledMessage,
    const QString& completedButCanceledMessage,
    const std::function<void()>& successAction)
{
    if (!hooks_.isActivationInProgress()) {
        hooks_.keepChecklistForUserDismissal(step, canceledMessage);
        hooks_.cancelActivation();
        hooks_.requestStatusSync();
        return;
    }
    if (!result.success) {
        hooks_.setCheckpointStatus(CoreStartupCheckpointStatus::Failed, step, result.message);
        hooks_.failStartup(result);
        return;
    }
    if (hooks_.configuredSystemProxyMode() != SystemProxyMode::ForcedChange) {
        hooks_.keepChecklistForUserDismissal(step, completedButCanceledMessage);
        hooks_.cancelActivation();
        hooks_.requestStatusSync();
        return;
    }

    if (successAction) {
        successAction();
    }
    hooks_.setCheckpointStatus(CoreStartupCheckpointStatus::Passed, step, result.message);
    hooks_.resumeStartup(skipTunCleanup, showStartupOverlay);
}
