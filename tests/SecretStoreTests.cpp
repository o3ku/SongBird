#include <QtTest>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <memory>

#include "domain/models/Config.h"
#include "persistence/JsonConfigRepository.h"
#include "persistence/SecretFieldCodec.h"
#include "platform/ISecretStore.h"
#include "platform/null/NullSecretStore.h"

namespace {

// A reversible stand-in for the platform stores: protect() marks the value
// with the production prefix and base64, unprotect() reverses it. Anything
// without the marker fails, which is what the undecryptable-value cases use.
class FakeSecretStore final : public ISecretStore {
public:
    bool available() const override { return true; }

    QString protect(const QString& plaintext) const override
    {
        if (plaintext.isEmpty()) {
            return {};
        }
        return SecretStoreFormat::protectedPrefix() + QStringLiteral("fake:")
            + QString::fromLatin1(plaintext.toUtf8().toBase64());
    }

    QString unprotect(const QString& stored) const override
    {
        if (!SecretStoreFormat::isProtectedValue(stored)) {
            return {};
        }
        const QString payload = SecretStoreFormat::stripProtectedPrefix(stored);
        if (!payload.startsWith(QStringLiteral("fake:"))) {
            return {};
        }
        const QByteArray decoded = QByteArray::fromBase64(payload.mid(5).toLatin1());
        if (decoded.isEmpty()) {
            return {};
        }
        return QString::fromUtf8(decoded);
    }
};

// A store that is present but cannot protect anything: every credential
// stays plaintext, and the failure must be recorded, not silent.
class FailingSecretStore final : public ISecretStore {
public:
    bool available() const override { return true; }
    QString protect(const QString&) const override { return {}; }
    QString unprotect(const QString&) const override { return {}; }
};

QString makeConfigPath(QTemporaryDir& tempDir)
{
    return tempDir.filePath(QStringLiteral("songbird.json"));
}

bool writeJsonFile(const QString& path, const QJsonObject& root)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    return file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0;
}

QJsonObject readJsonFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object() : QJsonObject();
}

QJsonObject makeLegacyRoot()
{
    QJsonObject server;
    server.insert(QStringLiteral("indexId"), QStringLiteral("server-1"));
    server.insert(QStringLiteral("configType"), 5);
    server.insert(QStringLiteral("address"), QStringLiteral("example.com"));
    server.insert(QStringLiteral("port"), 443);
    server.insert(QStringLiteral("id"), QStringLiteral("secret-uuid"));
    server.insert(QStringLiteral("security"), QStringLiteral("auto"));
    server.insert(QStringLiteral("publicKey"), QStringLiteral("public-not-secret"));
    server.insert(QStringLiteral("privateKey"), QStringLiteral("wg-private-key"));
    server.insert(QStringLiteral("obfsPassword"), QStringLiteral("hysteria-pass"));

    QJsonObject inbound;
    inbound.insert(QStringLiteral("localPort"), 10808);
    inbound.insert(QStringLiteral("pass"), QStringLiteral("inbound-pass"));

    QJsonObject root;
    root.insert(QStringLiteral("servers"), QJsonArray{server});
    root.insert(QStringLiteral("inbound"), QJsonArray{inbound});
    return root;
}

QString serverField(const QJsonObject& root, const QString& key)
{
    return root.value(QStringLiteral("servers")).toArray().at(0).toObject().value(key).toString();
}

QString inboundPassword(const QJsonObject& root)
{
    return root.value(QStringLiteral("inbound")).toArray().at(0).toObject().value(QStringLiteral("pass")).toString();
}

} // namespace

class SecretStoreTests : public QObject {
    Q_OBJECT

private slots:
    void codecRoundTripsServerSecretsAndInboundPassword();
    void codecLeavesNonSecretFieldsAlone();
    void repositoryMigratesLegacyPlaintextSecretsOnLoad();
    void repositoryKeepsPlaintextAndWarnsWhenNoStoreIsAvailable();
    void repositoryWarnsAndKeepsPlaintextWhenProtectFails();
    void repositoryIgnoresUndecryptableValuesAndWarns();
    void repositoryDoesNotDoubleEncryptProtectedValues();
};

void SecretStoreTests::codecRoundTripsServerSecretsAndInboundPassword()
{
    const FakeSecretStore store;

    QJsonObject root = makeLegacyRoot();
    const SecretFieldCodec::SaveReport saveReport = SecretFieldCodec::encodeSecrets(root, &store);
    QVERIFY(!saveReport.storeUnavailable);
    QVERIFY(!saveReport.secretProtectFailed);

    QVERIFY(SecretStoreFormat::isProtectedValue(serverField(root, QStringLiteral("id"))));
    QVERIFY(SecretStoreFormat::isProtectedValue(serverField(root, QStringLiteral("privateKey"))));
    QVERIFY(SecretStoreFormat::isProtectedValue(serverField(root, QStringLiteral("obfsPassword"))));
    QVERIFY(SecretStoreFormat::isProtectedValue(inboundPassword(root)));
    QVERIFY(serverField(root, QStringLiteral("id")) != QStringLiteral("secret-uuid"));

    // Encoding twice must not wrap the value again: the stored form is
    // already protected, so the second pass leaves it alone.
    const QString onceProtected = serverField(root, QStringLiteral("id"));
    SecretFieldCodec::encodeSecrets(root, &store);
    QCOMPARE(serverField(root, QStringLiteral("id")), onceProtected);

    const SecretFieldCodec::LoadReport loadReport = SecretFieldCodec::decodeSecrets(root, &store);
    QVERIFY(!loadReport.legacyPlaintextSecretsFound);
    QVERIFY(!loadReport.secretDecryptFailed);
    QVERIFY(!loadReport.storeUnavailable);
    QCOMPARE(serverField(root, QStringLiteral("id")), QStringLiteral("secret-uuid"));
    QCOMPARE(serverField(root, QStringLiteral("privateKey")), QStringLiteral("wg-private-key"));
    QCOMPARE(serverField(root, QStringLiteral("obfsPassword")), QStringLiteral("hysteria-pass"));
    QCOMPARE(inboundPassword(root), QStringLiteral("inbound-pass"));
}

void SecretStoreTests::codecLeavesNonSecretFieldsAlone()
{
    const FakeSecretStore store;

    QJsonObject root = makeLegacyRoot();
    SecretFieldCodec::encodeSecrets(root, &store);

    // A cipher name and a public key are not credentials: they must stay
    // readable in the file, or the config stops being portable for no gain.
    QCOMPARE(serverField(root, QStringLiteral("security")), QStringLiteral("auto"));
    QCOMPARE(serverField(root, QStringLiteral("publicKey")), QStringLiteral("public-not-secret"));
    QCOMPARE(serverField(root, QStringLiteral("address")), QStringLiteral("example.com"));

    const SecretFieldCodec::LoadReport report = SecretFieldCodec::decodeSecrets(root, &store);
    QVERIFY(!report.secretDecryptFailed);
    QCOMPARE(serverField(root, QStringLiteral("security")), QStringLiteral("auto"));
    QCOMPARE(serverField(root, QStringLiteral("publicKey")), QStringLiteral("public-not-secret"));
}

void SecretStoreTests::repositoryMigratesLegacyPlaintextSecretsOnLoad()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString configPath = makeConfigPath(tempDir);
    QVERIFY(writeJsonFile(configPath, makeLegacyRoot()));

    JsonConfigRepository repository(configPath, std::make_unique<FakeSecretStore>());
    const Config config = repository.load();

    QVERIFY(repository.lastLoadError().isEmpty());
    QVERIFY(repository.lastSecretStoreWarning().isEmpty());
    QCOMPARE(config.collection().servers.size(), 1);
    QCOMPARE(config.collection().servers.at(0).id, QStringLiteral("secret-uuid"));
    QCOMPARE(config.collection().servers.at(0).privateKey, QStringLiteral("wg-private-key"));
    QCOMPARE(config.collection().servers.at(0).obfsPassword, QStringLiteral("hysteria-pass"));
    QCOMPARE(config.inboundPassword, QStringLiteral("inbound-pass"));

    // The load re-saved the file: no plaintext credential may remain in it.
    const QJsonObject stored = readJsonFile(configPath);
    QVERIFY(SecretStoreFormat::isProtectedValue(serverField(stored, QStringLiteral("id"))));
    QVERIFY(SecretStoreFormat::isProtectedValue(serverField(stored, QStringLiteral("privateKey"))));
    QVERIFY(SecretStoreFormat::isProtectedValue(serverField(stored, QStringLiteral("obfsPassword"))));
    QVERIFY(SecretStoreFormat::isProtectedValue(inboundPassword(stored)));
}

void SecretStoreTests::repositoryKeepsPlaintextAndWarnsWhenNoStoreIsAvailable()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString configPath = makeConfigPath(tempDir);
    QVERIFY(writeJsonFile(configPath, makeLegacyRoot()));

    JsonConfigRepository repository(configPath, std::make_unique<NullSecretStore>());
    const Config config = repository.load();

    QVERIFY(repository.lastLoadError().isEmpty());
    QVERIFY(!repository.lastSecretStoreWarning().isEmpty());
    QCOMPARE(config.collection().servers.at(0).id, QStringLiteral("secret-uuid"));

    // Without a store there is nothing to migrate to: a save keeps the
    // plaintext value, and still says so.
    QVERIFY(repository.save(config));
    QVERIFY(!repository.lastSecretStoreWarning().isEmpty());
    QCOMPARE(serverField(readJsonFile(configPath), QStringLiteral("id")), QStringLiteral("secret-uuid"));
}

void SecretStoreTests::repositoryWarnsAndKeepsPlaintextWhenProtectFails()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    Config config;
    VmessItem server;
    server.indexId = QStringLiteral("server-1");
    server.configType = ConfigType::VLESS;
    server.address = QStringLiteral("example.com");
    server.port = 443;
    server.id = QStringLiteral("secret-uuid");
    config.collection().servers.append(server);

    JsonConfigRepository repository(makeConfigPath(tempDir), std::make_unique<FailingSecretStore>());
    QVERIFY(repository.save(config));

    // The credential must not be dropped just because it could not be
    // protected -- losing the node is worse than storing it plaintext -- but
    // the failure must be recorded where the app can surface it.
    QVERIFY(!repository.lastSecretStoreWarning().isEmpty());
    QCOMPARE(serverField(readJsonFile(makeConfigPath(tempDir)), QStringLiteral("id")), QStringLiteral("secret-uuid"));
}

void SecretStoreTests::repositoryIgnoresUndecryptableValuesAndWarns()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString configPath = makeConfigPath(tempDir);

    QJsonObject root = makeLegacyRoot();
    QJsonArray servers = root.value(QStringLiteral("servers")).toArray();
    QJsonObject server = servers.at(0).toObject();
    // A protected value this store cannot read: another platform's
    // ciphertext, or a truncated one. It must not become the password.
    server.insert(QStringLiteral("id"), QStringLiteral("enc:v1:not-a-fake-payload"));
    servers[0] = server;
    root.insert(QStringLiteral("servers"), servers);
    QVERIFY(writeJsonFile(configPath, root));

    JsonConfigRepository repository(configPath, std::make_unique<FakeSecretStore>());
    const Config config = repository.load();

    QVERIFY(repository.lastLoadError().isEmpty());
    QVERIFY(!repository.lastSecretStoreWarning().isEmpty());
    QCOMPARE(config.collection().servers.at(0).id, QString());

    // The migration must not run over a decrypt failure: re-saving would
    // write the undecryptable value away as an empty field.
    QCOMPARE(serverField(readJsonFile(configPath), QStringLiteral("id")), QStringLiteral("enc:v1:not-a-fake-payload"));
}

void SecretStoreTests::repositoryDoesNotDoubleEncryptProtectedValues()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString configPath = makeConfigPath(tempDir);
    QVERIFY(writeJsonFile(configPath, makeLegacyRoot()));

    JsonConfigRepository repository(configPath, std::make_unique<FakeSecretStore>());
    const Config firstLoad = repository.load();
    QVERIFY(repository.lastLoadError().isEmpty());

    QVERIFY(repository.save(firstLoad));
    const QString storedAfterFirstSave = serverField(readJsonFile(configPath), QStringLiteral("id"));
    QVERIFY(SecretStoreFormat::isProtectedValue(storedAfterFirstSave));

    const Config secondLoad = repository.load();
    QCOMPARE(secondLoad.collection().servers.at(0).id, QStringLiteral("secret-uuid"));
    QVERIFY(repository.save(secondLoad));
    QCOMPARE(serverField(readJsonFile(configPath), QStringLiteral("id")), storedAfterFirstSave);
}

QTEST_MAIN(SecretStoreTests)

#include "SecretStoreTests.moc"
