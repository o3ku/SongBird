#include <QtTest>

#include <optional>
#include <utility>

#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <QTemporaryDir>

#include "app/ServerCollectionCoordinator.h"
#include "persistence/IConfigRepository.h"
#include "services/ServerService.h"
#include "services/SubscriptionService.h"

namespace {

class MockConfigRepository : public IConfigRepository {
public:
    Config load() override { return config_; }

    bool save(const Config& config) override
    {
        ++saveCalls_;
        if (!saveSucceeds_) {
            // Mirrors the real repository for the failure case: lastSaveError() stays empty in a
            // test double, so the caller reports its own context sentence.
            return false;
        }
        config_ = config;
        return true;
    }

    QString lastLoadError() const override { return {}; }
    QString lastSaveError() const override { return {}; }

    Config config_;
    bool saveSucceeds_ = true;
    int saveCalls_ = 0;
};

const QString kId1 = QStringLiteral("id-1");
const QString kLockedManagedFile = QStringLiteral("locked-managed-config.json");

// A config whose active server is a Custom entry stored in managed storage, which is what makes a
// removal touch a file on disk at all.
Config makeConfigWithActiveCustomServer(const QString& address)
{
    Config config;
    VmessItem item;
    item.indexId = kId1;
    item.configType = ConfigType::Custom;
    item.address = address;
    config.collection().servers.append(item);
    config.currentIndexId = kId1;
    return config;
}

// Records what the coordinator asked the app to do. The coordinator owns no I/O of its own, so
// these callbacks are the whole observable surface of a removal.
struct RemovalRecorder {
    ServerCollectionCoordinator::Callbacks callbacks()
    {
        ServerCollectionCoordinator::Callbacks callbacks;
        callbacks.resolveActiveServer = [this]() { return resolveActiveServer(); };
        callbacks.isCoreRunning = [this]() { return coreRunning; };
        callbacks.appendResult = [this](const OperationResult& result) {
            messages.append(result.message);
        };
        callbacks.syncWindow = [this]() { ++syncWindowCalls; };
        callbacks.stopCore = [this](bool) { ++stopCoreCalls; };
        callbacks.restartCoreIfRunning = [this](const QString&, bool) { ++restartCoreCalls; };
        return callbacks;
    }

    QString log() const { return messages.join(QChar('\n')); }

    // The coordinator resolves the active server from the config, exactly as AppBootstrap does.
    std::optional<VmessItem> resolveActiveServer() const
    {
        if (config == nullptr) {
            return std::nullopt;
        }
        for (const VmessItem& item : config->collection().servers) {
            if (item.indexId == config->currentIndexId) {
                return item;
            }
        }
        return std::nullopt;
    }

    const Config* config = nullptr;
    bool coreRunning = true;
    QStringList messages;
    int syncWindowCalls = 0;
    int stopCoreCalls = 0;
    int restartCoreCalls = 0;
};

} // namespace

// Why these tests exist: ServerService::removeServers() writes the config and *then* deletes the
// managed custom config files, and it reports a failure when a file survived. The coordinator used
// to gate its core handling on that failure, so a leaked file also skipped the stop/reload -- the
// core kept running the server the user had just deleted. It now gates on whether the removal was
// applied; these three cases pin that from both sides.
class ServerCollectionCoordinatorTests : public QObject {
    Q_OBJECT

private slots:
    void stopsTheCoreWhenTheActiveServerIsRemovedCleanly();
    void stopsTheCoreWhenOnlyTheManagedFileCleanupFailed();
    void keepsTheCoreRunningWhenTheRemovalCouldNotBeSaved();
    void hidingASubscriptionDoesNotSaveOnItsOwn();
    void persistingPendingEditsWritesTheHiddenStateOnce();
};

void ServerCollectionCoordinatorTests::stopsTheCoreWhenTheActiveServerIsRemovedCleanly()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    // No file sits at the managed path, so removeManagedConfig() has nothing to delete and reports
    // success -- the ordinary removal.
    Config config = makeConfigWithActiveCustomServer(QStringLiteral("clean-managed-config.json"));
    MockConfigRepository repository;
    ServerService serverService(repository, QDir(temporaryDirectory.path()).filePath(QStringLiteral("managed")));
    SubscriptionService subscriptionService(repository);

    RemovalRecorder recorder;
    recorder.config = &config;

    ServerCollectionCoordinator coordinator(
        ServerCollectionCoordinator::Dependencies{config, serverService, subscriptionService},
        recorder.callbacks());

    coordinator.removeServers({kId1});

    QVERIFY(recorder.log().contains(QStringLiteral("Server selection removed.")));
    QVERIFY(!recorder.log().contains(QStringLiteral("could not all be deleted")));
    QCOMPARE(recorder.stopCoreCalls, 1);
    QVERIFY(recorder.log().contains(
        QStringLiteral("Stopping core because the active server was removed.")));
}

void ServerCollectionCoordinatorTests::stopsTheCoreWhenOnlyTheManagedFileCleanupFailed()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString managedDirectory = QDir(temporaryDirectory.path()).filePath(QStringLiteral("managed"));
    // A non-empty directory sitting at the managed path is the portable way to make QFile::remove()
    // fail: no platform lets a file deletion remove a directory. The config write still succeeds, so
    // the removal is applied while the cleanup is not -- the exact combination that used to leave
    // the core running the deleted server.
    const QString lockedPath = QDir(managedDirectory).filePath(kLockedManagedFile);
    QVERIFY(QDir().mkpath(QDir(lockedPath).filePath(QStringLiteral("keep"))));

    Config config = makeConfigWithActiveCustomServer(kLockedManagedFile);
    MockConfigRepository repository;
    ServerService serverService(repository, managedDirectory);
    SubscriptionService subscriptionService(repository);

    RemovalRecorder recorder;
    recorder.config = &config;

    ServerCollectionCoordinator coordinator(
        ServerCollectionCoordinator::Dependencies{config, serverService, subscriptionService},
        recorder.callbacks());

    coordinator.removeServers({kId1});

    // The leaked file is still reported as a failure, and it names the file.
    QVERIFY(recorder.log().contains(QStringLiteral("could not all be deleted")));
    QVERIFY(recorder.log().contains(QDir::toNativeSeparators(QFileInfo(lockedPath).absoluteFilePath())));
    QVERIFY(QFileInfo::exists(lockedPath));
    // The removal itself was applied, so the core work must run anyway.
    QCOMPARE(repository.saveCalls_, 1);
    QCOMPARE(recorder.stopCoreCalls, 1);
    QVERIFY(recorder.log().contains(
        QStringLiteral("Stopping core because the active server was removed.")));
}

void ServerCollectionCoordinatorTests::keepsTheCoreRunningWhenTheRemovalCouldNotBeSaved()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    Config config = makeConfigWithActiveCustomServer(QStringLiteral("unsaved-managed-config.json"));
    MockConfigRepository repository;
    repository.saveSucceeds_ = false;
    ServerService serverService(repository, QDir(temporaryDirectory.path()).filePath(QStringLiteral("managed")));
    SubscriptionService subscriptionService(repository);

    RemovalRecorder recorder;
    recorder.config = &config;

    ServerCollectionCoordinator coordinator(
        ServerCollectionCoordinator::Dependencies{config, serverService, subscriptionService},
        recorder.callbacks());

    coordinator.removeServers({kId1});

    // Counterpart to the case above: nothing was persisted, so the core must be left alone rather
    // than stopped on the strength of an in-memory edit that never reached the config file.
    QVERIFY(recorder.log().contains(
        QStringLiteral("Failed to save configuration after removing server(s).")));
    QCOMPARE(recorder.stopCoreCalls, 0);
    QCOMPARE(recorder.restartCoreCalls, 0);
}

void ServerCollectionCoordinatorTests::hidingASubscriptionDoesNotSaveOnItsOwn()
{
    // Hiding is the first half of "hide, reorder, show" -- the only route to moving a subscription
    // that has no tab. Saving on the hide wrote the file once for the hide and again for the move,
    // so the intermediate write landed a list the user was still rearranging. The edit is applied in
    // memory here, and the save belongs to whatever ends the gesture.
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    Config config;
    SubItem visible;
    visible.id = QStringLiteral("sub-visible");
    visible.remarks = QStringLiteral("Visible");
    visible.url = QStringLiteral("https://a.example");
    visible.enabled = true;
    config.collection().subscriptions.append(visible);

    SubItem hidden;
    hidden.id = QStringLiteral("sub-hidden");
    hidden.remarks = QStringLiteral("Hidden");
    hidden.url = QStringLiteral("https://b.example");
    hidden.enabled = false;
    config.collection().subscriptions.append(hidden);
    config.ui().mainSelectedSubId = visible.id;

    MockConfigRepository repository;
    ServerService serverService(repository, QDir(temporaryDirectory.path()).filePath(QStringLiteral("managed")));
    SubscriptionService subscriptionService(repository);

    RemovalRecorder recorder;
    recorder.config = &config;

    ServerCollectionCoordinator coordinator(
        ServerCollectionCoordinator::Dependencies{config, serverService, subscriptionService},
        recorder.callbacks());

    coordinator.hideSubscription(visible.id);

    QVERIFY(!config.collection().subscriptions.at(0).enabled);
    QVERIFY(config.ui().mainSelectedSubId.isEmpty());
    QCOMPARE(repository.saveCalls_, 0);
    // The tabs are redrawn from the in-memory edit, so the click still looks like it did something.
    QCOMPARE(recorder.syncWindowCalls, 1);
    QVERIFY(recorder.log().contains(QStringLiteral("hidden")));
}

void ServerCollectionCoordinatorTests::persistingPendingEditsWritesTheHiddenStateOnce()
{
    // The other half of the deferred save: hiding leaves the file alone, so something has to write it
    // before the process ends, and that something is this. "Hide, reorder, show" therefore costs one
    // write of the list the user settled on -- not one per click, and not zero.
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    Config config;
    SubItem first;
    first.id = QStringLiteral("sub-a");
    first.remarks = QStringLiteral("A");
    first.url = QStringLiteral("https://a.example");
    first.enabled = true;
    config.collection().subscriptions.append(first);

    SubItem second;
    second.id = QStringLiteral("sub-b");
    second.remarks = QStringLiteral("B");
    second.url = QStringLiteral("https://b.example");
    second.enabled = true;
    config.collection().subscriptions.append(second);
    config.ui().mainSelectedSubId = first.id;

    MockConfigRepository repository;
    ServerService serverService(repository, QDir(temporaryDirectory.path()).filePath(QStringLiteral("managed")));
    SubscriptionService subscriptionService(repository);

    RemovalRecorder recorder;
    recorder.config = &config;

    ServerCollectionCoordinator coordinator(
        ServerCollectionCoordinator::Dependencies{config, serverService, subscriptionService},
        recorder.callbacks());

    coordinator.hideSubscription(first.id);
    QCOMPARE(repository.saveCalls_, 0);

    coordinator.persistPendingSubscriptionEdits();

    QCOMPARE(repository.saveCalls_, 1);
    QVERIFY(!config.collection().subscriptions.at(0).enabled);
    // What reached the file is the hidden state, not the list from before the click.
    QVERIFY(!repository.config_.collection().subscriptions.at(0).enabled);

    // A failed checkpoint is reported, not swallowed: this is the only write between the click and
    // the exit, so silence here would be a hide that silently never happened.
    repository.saveSucceeds_ = false;
    RemovalRecorder failingRecorder;
    failingRecorder.config = &config;
    ServerCollectionCoordinator failingCoordinator(
        ServerCollectionCoordinator::Dependencies{config, serverService, subscriptionService},
        failingRecorder.callbacks());
    failingCoordinator.persistPendingSubscriptionEdits();

    QCOMPARE(repository.saveCalls_, 2);
    QVERIFY(failingRecorder.log().contains(QStringLiteral("Failed to save")));
}

QTEST_MAIN(ServerCollectionCoordinatorTests)

#include "ServerCollectionCoordinatorTests.moc"
