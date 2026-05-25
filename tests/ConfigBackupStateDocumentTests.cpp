#include <QtTest>

#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>

#include "services/ConfigBackupStateDocument.h"

class ConfigBackupStateDocumentTests : public QObject {
    Q_OBJECT

private slots:
    void stateConfigPathUsesPrimaryBaseName();
    void mergeStateIntoPrimaryCopiesUiAndMatchingServerState();
    void extractStateFromMergedRootRemovesStateOnlyFields();
};

void ConfigBackupStateDocumentTests::stateConfigPathUsesPrimaryBaseName()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString configPath = QDir(directory.path()).filePath(QStringLiteral("songbird.json"));
    QCOMPARE(
        ConfigBackupStateDocument::stateConfigPathFor(configPath),
        QDir(directory.path()).filePath(QStringLiteral("songbird.state.json")));
}

void ConfigBackupStateDocumentTests::mergeStateIntoPrimaryCopiesUiAndMatchingServerState()
{
    QJsonObject ui;
    ui.insert(QStringLiteral("theme"), QStringLiteral("dark"));

    QJsonObject firstServer;
    firstServer.insert(QStringLiteral("indexId"), QStringLiteral("server-a"));
    firstServer.insert(QStringLiteral("address"), QStringLiteral("127.0.0.1"));

    QJsonObject secondServer;
    secondServer.insert(QStringLiteral("indexId"), QStringLiteral("server-b"));
    secondServer.insert(QStringLiteral("address"), QStringLiteral("127.0.0.2"));

    QJsonArray servers;
    servers.append(firstServer);
    servers.append(secondServer);

    QJsonObject primaryRoot;
    primaryRoot.insert(QStringLiteral("ui"), ui);
    primaryRoot.insert(QStringLiteral("servers"), servers);

    QJsonObject stateUi;
    stateUi.insert(QStringLiteral("mainProxyEnabled"), true);

    QJsonObject serverState;
    serverState.insert(QStringLiteral("indexId"), QStringLiteral("server-b"));
    serverState.insert(QStringLiteral("testResult"), QStringLiteral("42ms"));

    QJsonObject stateRoot;
    stateRoot.insert(QStringLiteral("ui"), stateUi);
    stateRoot.insert(QStringLiteral("serverStates"), QJsonArray{serverState});

    ConfigBackupStateDocument::mergeStateIntoPrimary(primaryRoot, stateRoot);

    const QJsonObject mergedUi = primaryRoot.value(QStringLiteral("ui")).toObject();
    QCOMPARE(mergedUi.value(QStringLiteral("theme")).toString(), QStringLiteral("dark"));
    QCOMPARE(mergedUi.value(QStringLiteral("mainProxyEnabled")).toBool(), true);

    const QJsonArray mergedServers = primaryRoot.value(QStringLiteral("servers")).toArray();
    QVERIFY(!mergedServers.at(0).toObject().contains(QStringLiteral("testResult")));
    QCOMPARE(
        mergedServers.at(1).toObject().value(QStringLiteral("testResult")).toString(),
        QStringLiteral("42ms"));
}

void ConfigBackupStateDocumentTests::extractStateFromMergedRootRemovesStateOnlyFields()
{
    QJsonObject mainColumnWidths;
    mainColumnWidths.insert(QStringLiteral("address"), 180);

    QJsonObject ui;
    ui.insert(QStringLiteral("theme"), QStringLiteral("dark"));
    ui.insert(QStringLiteral("mainLocationX"), 12);
    ui.insert(QStringLiteral("mainProxyEnabled"), true);
    ui.insert(QStringLiteral("mainColumnWidths"), mainColumnWidths);

    QJsonObject server;
    server.insert(QStringLiteral("indexId"), QStringLiteral("server-a"));
    server.insert(QStringLiteral("address"), QStringLiteral("127.0.0.1"));
    server.insert(QStringLiteral("testResult"), QStringLiteral("88ms"));

    QJsonObject primaryRoot;
    primaryRoot.insert(QStringLiteral("ui"), ui);
    primaryRoot.insert(QStringLiteral("servers"), QJsonArray{server});

    const QJsonObject stateRoot = ConfigBackupStateDocument::extractStateFromMergedRoot(primaryRoot);

    const QJsonObject remainingUi = primaryRoot.value(QStringLiteral("ui")).toObject();
    QCOMPARE(remainingUi.size(), 1);
    QCOMPARE(remainingUi.value(QStringLiteral("theme")).toString(), QStringLiteral("dark"));

    const QJsonObject stateUi = stateRoot.value(QStringLiteral("ui")).toObject();
    QCOMPARE(stateUi.value(QStringLiteral("mainLocationX")).toInt(), 12);
    QCOMPARE(stateUi.value(QStringLiteral("mainProxyEnabled")).toBool(), true);
    QVERIFY(stateUi.value(QStringLiteral("mainColumnWidths")).isObject());

    const QJsonObject restoredServer = primaryRoot.value(QStringLiteral("servers")).toArray().at(0).toObject();
    QVERIFY(!restoredServer.contains(QStringLiteral("testResult")));

    const QJsonArray serverStates = stateRoot.value(QStringLiteral("serverStates")).toArray();
    QCOMPARE(serverStates.size(), 1);
    QCOMPARE(
        serverStates.at(0).toObject().value(QStringLiteral("testResult")).toString(),
        QStringLiteral("88ms"));
}

QTEST_MAIN(ConfigBackupStateDocumentTests)

#include "ConfigBackupStateDocumentTests.moc"
