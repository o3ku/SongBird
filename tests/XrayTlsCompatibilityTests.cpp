#include <QtTest>

#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QTranslator>

#include "runtime/ClientConfigWriter.h"
#include "runtime/XrayTlsCompatibility.h"
#include "services/SpeedTestBatchRuntimeRunner.h"
#include "services/SpeedTestRuntimeRunner.h"
#include "services/SpeedTestServiceInternal.h"

namespace {

QDateTime utcTime(int month, int day)
{
    return QDateTime(QDate(2026, month, day), QTime(12, 0), Qt::UTC);
}

QJsonObject rootWithTls(QJsonObject tls)
{
    return QJsonObject{{QStringLiteral("outbounds"), QJsonArray{QJsonObject{
        {QStringLiteral("tag"), QStringLiteral("proxy")},
        {QStringLiteral("streamSettings"), QJsonObject{
            {QStringLiteral("security"), QStringLiteral("tls")},
            {QStringLiteral("tlsSettings"), tls}}}}}}};
}

QJsonObject tlsSettings(const QJsonObject& root)
{
    return root.value(QStringLiteral("outbounds")).toArray().at(0).toObject()
        .value(QStringLiteral("streamSettings")).toObject().value(QStringLiteral("tlsSettings")).toObject();
}

QString pin(QChar digit)
{
    return QString(64, digit);
}

class TaggingTranslator : public QTranslator {
public:
    bool isEmpty() const override { return false; }
    QString translate(const char* context, const char* sourceText, const char*, int) const override
    {
        return qstrcmp(context, "XrayTlsCompatibility") == 0
            ? QStringLiteral("[zh]%1").arg(QString::fromUtf8(sourceText)) : QString();
    }
};

} // namespace

class XrayTlsCompatibilityTests : public QObject {
    Q_OBJECT

private slots:
    void legacyVersionsKeepSkippingVerification();
    void januaryRemovalRejectsSkippingBeforeTheGracePeriod();
    void graceVersionsRespectTheExactUtcDeadline();
    void juneRemovalRejectsSkippingEvenBeforeTheDeadline();
    void unsupportedSkippingFailsWithoutChangingTheConfig();
    void knownPinsDisableSkippingOnModernVersions();
    void earlyPinVersionsUseTheTildeSeparator();
    void pinsNormalizeColonFormattingAndRemoveDuplicates();
    void legacyVersionsRejectUnsupportedLeafPins();
    void malformedPinsFailWithoutChangingTheConfig();
    void unknownVersionRejectsSkippingAndPinning();
    void normalVerificationDoesNotRequireAVersion();
    void realityAndPlaintextStayUnchanged();
    void failureMessagesUseTheTranslator();
    void nativeCoreMatchesTheRuntimeCompatibilityChecks();
};

void XrayTlsCompatibilityTests::legacyVersionsKeepSkippingVerification()
{
    for (const QString& version : {QStringLiteral("1.8.24"), QStringLiteral("25.12.8"),
             QStringLiteral("26.1.13"), QStringLiteral("v26.1.23")}) {
        QVERIFY(XrayTlsCompatibility::supportsAllowInsecure(version, utcTime(10, 16)));
        QJsonObject root = rootWithTls({{QStringLiteral("allowInsecure"), true}});
        const auto result = XrayTlsCompatibility::adaptConfig(root, version, utcTime(10, 16));
        QVERIFY2(result.success, qPrintable(result.message));
        QVERIFY(tlsSettings(root).value(QStringLiteral("allowInsecure")).toBool());
    }
}

void XrayTlsCompatibilityTests::januaryRemovalRejectsSkippingBeforeTheGracePeriod()
{
    QVERIFY(!XrayTlsCompatibility::supportsAllowInsecure(QStringLiteral("26.1.31"), utcTime(2, 1)));
    QVERIFY(!XrayTlsCompatibility::supportsAllowInsecure(QStringLiteral("26.2.2"), utcTime(2, 3)));
}

void XrayTlsCompatibilityTests::graceVersionsRespectTheExactUtcDeadline()
{
    const QDateTime deadline(QDate(2026, 6, 1), QTime(0, 0), Qt::UTC);
    for (const QString& version : {QStringLiteral("26.2.4"), QStringLiteral("26.3.27"), QStringLiteral("26.6.1")}) {
        QVERIFY(XrayTlsCompatibility::supportsAllowInsecure(version, deadline.addSecs(-1)));
        QVERIFY(XrayTlsCompatibility::supportsAllowInsecure(version, deadline));
        QVERIFY(!XrayTlsCompatibility::supportsAllowInsecure(version, deadline.addSecs(1)));
        QVERIFY(!XrayTlsCompatibility::supportsAllowInsecure(version, {}));
    }
}

void XrayTlsCompatibilityTests::juneRemovalRejectsSkippingEvenBeforeTheDeadline()
{
    QVERIFY(!XrayTlsCompatibility::supportsAllowInsecure(QStringLiteral("26.6.22"), utcTime(5, 1)));
    QVERIFY(!XrayTlsCompatibility::supportsAllowInsecure(QStringLiteral("26.10.10"), utcTime(5, 1)));
}

void XrayTlsCompatibilityTests::unsupportedSkippingFailsWithoutChangingTheConfig()
{
    QJsonObject root = rootWithTls({{QStringLiteral("allowInsecure"), true}});
    const QJsonObject original = root;
    const auto result = XrayTlsCompatibility::adaptConfig(root, QStringLiteral("26.3.27"), utcTime(10, 16));
    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("26.3.27")));
    QVERIFY(result.message.contains(QStringLiteral("sing-box")));
    QCOMPARE(root, original);
}

void XrayTlsCompatibilityTests::knownPinsDisableSkippingOnModernVersions()
{
    QJsonObject root = rootWithTls({{QStringLiteral("allowInsecure"), true},
        {QStringLiteral("pinnedPeerCertSha256"), pin(QChar('a')) + QChar('~') + pin(QChar('b'))}});
    const auto result = XrayTlsCompatibility::adaptConfig(root, QStringLiteral("26.3.27"), utcTime(10, 16));
    QVERIFY2(result.success, qPrintable(result.message));
    QVERIFY(!tlsSettings(root).value(QStringLiteral("allowInsecure")).toBool());
    QCOMPARE(tlsSettings(root).value(QStringLiteral("pinnedPeerCertSha256")).toString(),
        pin(QChar('a')) + QChar(',') + pin(QChar('b')));
}

void XrayTlsCompatibilityTests::earlyPinVersionsUseTheTildeSeparator()
{
    QJsonObject root = rootWithTls({{QStringLiteral("pinnedPeerCertSha256"),
        pin(QChar('a')) + QChar(',') + pin(QChar('b'))}});
    const auto result = XrayTlsCompatibility::adaptConfig(root, QStringLiteral("26.1.23"), utcTime(10, 16));
    QVERIFY2(result.success, qPrintable(result.message));
    QCOMPARE(tlsSettings(root).value(QStringLiteral("pinnedPeerCertSha256")).toString(),
        pin(QChar('a')) + QChar('~') + pin(QChar('b')));
}

void XrayTlsCompatibilityTests::pinsNormalizeColonFormattingAndRemoveDuplicates()
{
    QStringList octets;
    for (int i = 0; i < 32; ++i) {
        octets.append(QStringLiteral("AB"));
    }
    QJsonObject root = rootWithTls({{QStringLiteral("pinnedPeerCertSha256"),
        octets.join(QChar(':')) + QChar(',') + QStringLiteral("ab").repeated(32)}});
    QVERIFY(XrayTlsCompatibility::adaptConfig(root, QStringLiteral("26.1.31"), utcTime(10, 16)).success);
    QCOMPARE(tlsSettings(root).value(QStringLiteral("pinnedPeerCertSha256")).toString(), QStringLiteral("ab").repeated(32));
}

void XrayTlsCompatibilityTests::legacyVersionsRejectUnsupportedLeafPins()
{
    QJsonObject root = rootWithTls({{QStringLiteral("pinnedPeerCertSha256"), pin(QChar('a'))}});
    const auto result = XrayTlsCompatibility::adaptConfig(root, QStringLiteral("25.12.8"), utcTime(10, 16));
    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("26.1.13")));
}

void XrayTlsCompatibilityTests::malformedPinsFailWithoutChangingTheConfig()
{
    for (const QString& value : {QStringLiteral("not-a-hash"), pin(QChar('a')) + QChar(','), QString(64, QChar('z'))}) {
        QJsonObject root = rootWithTls({{QStringLiteral("pinnedPeerCertSha256"), value}});
        const QJsonObject original = root;
        QVERIFY(!XrayTlsCompatibility::adaptConfig(root, QStringLiteral("26.3.27"), utcTime(10, 16)).success);
        QCOMPARE(root, original);
    }
}

void XrayTlsCompatibilityTests::unknownVersionRejectsSkippingAndPinning()
{
    QJsonObject root = rootWithTls({{QStringLiteral("allowInsecure"), true}});
    QVERIFY(!XrayTlsCompatibility::adaptConfig(root, QString(), utcTime(10, 16)).success);
    root = rootWithTls({{QStringLiteral("pinnedPeerCertSha256"), pin(QChar('a'))}});
    QVERIFY(!XrayTlsCompatibility::adaptConfig(root, QStringLiteral("unknown"), utcTime(10, 16)).success);
}

void XrayTlsCompatibilityTests::normalVerificationDoesNotRequireAVersion()
{
    QJsonObject root = rootWithTls({{QStringLiteral("allowInsecure"), false},
        {QStringLiteral("serverName"), QStringLiteral("tls.example.com")}});
    const QJsonObject original = root;
    QVERIFY(XrayTlsCompatibility::adaptConfig(root, {}, utcTime(10, 16)).success);
    QCOMPARE(root, original);
}

void XrayTlsCompatibilityTests::realityAndPlaintextStayUnchanged()
{
    QJsonObject root{{QStringLiteral("outbounds"), QJsonArray{
        QJsonObject{{QStringLiteral("protocol"), QStringLiteral("freedom")}},
        QJsonObject{{QStringLiteral("streamSettings"), QJsonObject{{QStringLiteral("security"), QStringLiteral("reality")}}}}}}};
    const QJsonObject original = root;
    QVERIFY(XrayTlsCompatibility::adaptConfig(root, {}, utcTime(10, 16)).success);
    QCOMPARE(root, original);
}

void XrayTlsCompatibilityTests::failureMessagesUseTheTranslator()
{
    TaggingTranslator translator;
    QCoreApplication::installTranslator(&translator);
    QJsonObject root = rootWithTls({{QStringLiteral("allowInsecure"), true}});
    const auto unknown = XrayTlsCompatibility::adaptConfig(root, {}, utcTime(10, 16));
    const auto removed = XrayTlsCompatibility::adaptConfig(root, QStringLiteral("26.3.27"), utcTime(10, 16));
    root = rootWithTls({{QStringLiteral("pinnedPeerCertSha256"), pin(QChar('a'))}});
    const auto unsupported = XrayTlsCompatibility::adaptConfig(root, QStringLiteral("25.12.8"), utcTime(10, 16));
    root = rootWithTls({{QStringLiteral("pinnedPeerCertSha256"), QStringLiteral("bad")}});
    const auto invalid = XrayTlsCompatibility::adaptConfig(root, QStringLiteral("26.3.27"), utcTime(10, 16));
    QCoreApplication::removeTranslator(&translator);
    for (const auto& result : {unknown, removed, unsupported, invalid}) {
        QVERIFY(!result.success);
        QVERIFY(result.message.startsWith(QStringLiteral("[zh]")));
    }
}

void XrayTlsCompatibilityTests::nativeCoreMatchesTheRuntimeCompatibilityChecks()
{
    const QString program = qEnvironmentVariable("SONGBIRD_TEST_XRAY_EXECUTABLE");
    if (program.isEmpty()) {
        QSKIP("Set SONGBIRD_TEST_XRAY_EXECUTABLE to validate a native Xray executable.");
    }
    QVERIFY(QFileInfo::exists(program));
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto nativeAccepts = [&](const QJsonObject& generatedRoot) {
        // 只校验真实生成的出站，排除路由数据库和外网环境对版本兼容测试的干扰。
        QJsonObject proxy;
        for (const auto& value : generatedRoot.value(QStringLiteral("outbounds")).toArray()) {
            if (value.toObject().value(QStringLiteral("tag")).toString() == QStringLiteral("proxy")) {
                proxy = value.toObject();
                break;
            }
        }
        if (proxy.isEmpty()) {
            return false;
        }
        QFile file(directory.filePath(QStringLiteral("native.json")));
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }
        file.write(QJsonDocument(QJsonObject{{QStringLiteral("outbounds"), QJsonArray{proxy}}}).toJson());
        file.close();
        QProcess process;
        process.setProgram(program);
        process.setArguments({QStringLiteral("run"), QStringLiteral("-test"), QStringLiteral("-config"), file.fileName()});
        process.start();
        if (!process.waitForStarted(2000) || !process.waitForFinished(5000)) {
            process.kill();
            process.waitForFinished(1000);
            return false;
        }
        return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    };
    Config config;
    config.policy().coreTypeItems.append(CoreTypeItem{
        static_cast<int>(ConfigType::Trojan), static_cast<int>(CoreType::Xray)});
    VmessItem server;
    server.configType = ConfigType::Trojan;
    server.address = QStringLiteral("example.com");
    server.port = 443;
    server.id = QStringLiteral("test-password");
    server.streamSecurity = QStringLiteral("tls");
    server.network = QStringLiteral("tcp");
    server.allowInsecure = QStringLiteral("true");
    ClientConfigWriter originalWriter;
    const bool nativeAllowsSkipping = nativeAccepts(originalWriter.generateClientConfigs(config, server).primary.root);
    CoreInfo coreInfo;
    coreInfo.type = CoreType::Xray;
    coreInfo.program = program;
    ClientConfigWriter writer;
    writer.setRuntimeCoreInfo(coreInfo);
    const auto skipping = writer.generateClientConfigs(config, server);
    QCOMPARE(skipping.result.success, nativeAllowsSkipping);
    if (!nativeAllowsSkipping) {
        const std::atomic_bool cancelled{false};
        SpeedTestRequestItem item;
        item.indexId = QStringLiteral("node");
        item.configType = ConfigType::Trojan;
        item.runtimeServer = server;
        item.coreInfo = coreInfo;
        int rejectedNodes = 0;
        const Config probeConfig = SpeedTestServiceInternal::makeUrlTestRuntimeConfig(config);
        const bool handled = SpeedTestBatchRuntimeRunner::runBatchedGroup(
            probeConfig, QStringLiteral("http://127.0.0.1/generate_204"), {item, item}, {}, cancelled,
            [](const QString&) {}, [&](const QString&, const QString& message) {
                QVERIFY(message.contains(QStringLiteral("Xray")));
                ++rejectedNodes;
            });
        QVERIFY(handled);
        QCOMPARE(rejectedNodes, 2);
        const QString singleResult = SpeedTestRuntimeRunner::runUrlTest(
            probeConfig, QStringLiteral("http://127.0.0.1/generate_204"), item, {}, cancelled, [](const QString&) {});
        QVERIFY(singleResult.contains(QStringLiteral("Xray")));
    }
    server.allowInsecure = QStringLiteral("false");
    const auto strict = writer.generateClientConfigs(config, server);
    QVERIFY2(strict.result.success, qPrintable(strict.result.message));
    QVERIFY(nativeAccepts(strict.primary.root));
    server.certSha = pin(QChar('a')) + QChar(',') + pin(QChar('b'));
    const auto pinned = writer.generateClientConfigs(config, server);
    if (pinned.result.success) {
        QVERIFY(nativeAccepts(pinned.primary.root));
    } else {
        QVERIFY(pinned.result.message.contains(QStringLiteral("26.1.13")));
    }
}

QTEST_MAIN(XrayTlsCompatibilityTests)
#include "XrayTlsCompatibilityTests.moc"
