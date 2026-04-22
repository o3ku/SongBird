#include <QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include "domain/models/Config.h"
#include "domain/models/VmessItem.h"
#include "runtime/ClashConfigWriter.h"

class ClashConfigWriterTests : public QObject {
    Q_OBJECT

private slots:
    void patchesTopLevelKeys();
    void appendsMissingKeys();
    void stripsSecretLine();
    void failsWhenSourceMissing();
    void idempotentOnSecondRun();
    void leavesNestedKeysUntouched();
    void preservesStrTagRoundTrip();

private:
    static QString readFile(const QString& path);
    static QString writeSampleYaml(const QTemporaryDir& dir, const QString& body);
};

QString ClashConfigWriterTests::readFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

QString ClashConfigWriterTests::writeSampleYaml(const QTemporaryDir& dir, const QString& body)
{
    const QString path = QDir(dir.path()).filePath(QStringLiteral("source.yaml"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return {};
    }
    file.write(body.toUtf8());
    file.close();
    return path;
}

void ClashConfigWriterTests::patchesTopLevelKeys()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = writeSampleYaml(dir, QStringLiteral(
        "mixed-port: 7890\n"
        "allow-lan: true\n"
        "log-level: info\n"
        "external-controller: 127.0.0.1:9999\n"
        "proxies: []\n"));

    Config config;
    config.localPort = 10808;
    config.allowLanConnection = false;
    config.logLevel = QStringLiteral("warning");

    VmessItem server;
    server.configType = ConfigType::Custom;
    server.coreType = CoreType::Clash;
    server.address = source;

    const QString output = QDir(dir.path()).filePath(QStringLiteral("out.yaml"));
    const OperationResult result = ClashConfigWriter::writeClientConfig(config, server, output, 12345);

    QVERIFY2(result.success, qUtf8Printable(result.message));

    const QString patched = readFile(output);
    QVERIFY(patched.contains(QStringLiteral("mixed-port: 10808")));
    QVERIFY(!patched.contains(QStringLiteral("mixed-port: 7890")));
    QVERIFY(patched.contains(QStringLiteral("allow-lan: false")));
    QVERIFY(patched.contains(QStringLiteral("log-level: warning")));
    QVERIFY(patched.contains(QStringLiteral("external-controller: \"127.0.0.1:12345\"")));
    QVERIFY(patched.contains(QStringLiteral("proxies: []")));
}

void ClashConfigWriterTests::appendsMissingKeys()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = writeSampleYaml(dir, QStringLiteral(
        "proxies:\n"
        "  - name: sample\n"));

    Config config;
    config.localPort = 7897;
    config.allowLanConnection = true;
    config.logLevel = QStringLiteral("info");

    VmessItem server;
    server.coreType = CoreType::ClashMeta;
    server.address = source;

    const QString output = QDir(dir.path()).filePath(QStringLiteral("out.yaml"));
    const OperationResult result = ClashConfigWriter::writeClientConfig(config, server, output, 9091);

    QVERIFY2(result.success, qUtf8Printable(result.message));

    const QString patched = readFile(output);
    QVERIFY(patched.contains(QStringLiteral("mixed-port: 7897")));
    QVERIFY(patched.contains(QStringLiteral("allow-lan: true")));
    QVERIFY(patched.contains(QStringLiteral("bind-address: \"*\"")));
    QVERIFY(patched.contains(QStringLiteral("external-controller: \"127.0.0.1:9091\"")));
    QVERIFY(patched.contains(QStringLiteral("proxies:")));
}

void ClashConfigWriterTests::stripsSecretLine()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = writeSampleYaml(dir, QStringLiteral(
        "mixed-port: 7890\n"
        "secret: \"keep-me-out\"\n"
        "proxies: []\n"));

    Config config;
    config.localPort = 10808;

    VmessItem server;
    server.coreType = CoreType::Clash;
    server.address = source;

    const QString output = QDir(dir.path()).filePath(QStringLiteral("out.yaml"));
    const OperationResult result = ClashConfigWriter::writeClientConfig(config, server, output, 9090);

    QVERIFY2(result.success, qUtf8Printable(result.message));
    const QString patched = readFile(output);
    QVERIFY(!patched.contains(QStringLiteral("keep-me-out")));
    QVERIFY(!patched.contains(QStringLiteral("secret:")));
}

void ClashConfigWriterTests::failsWhenSourceMissing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Config config;
    VmessItem server;
    server.coreType = CoreType::Clash;
    server.address = QDir(dir.path()).filePath(QStringLiteral("does-not-exist.yaml"));

    const QString output = QDir(dir.path()).filePath(QStringLiteral("out.yaml"));
    const OperationResult result = ClashConfigWriter::writeClientConfig(config, server, output, 9090);

    QVERIFY(!result.success);
}

void ClashConfigWriterTests::idempotentOnSecondRun()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = writeSampleYaml(dir, QStringLiteral(
        "mixed-port: 7890\n"
        "allow-lan: true\n"
        "external-controller: 127.0.0.1:9999\n"
        "log-level: info\n"
        "proxies: []\n"));

    Config config;
    config.localPort = 10808;
    config.allowLanConnection = false;
    config.logLevel = QStringLiteral("warning");

    VmessItem server;
    server.coreType = CoreType::Clash;
    server.address = source;

    const QString output = QDir(dir.path()).filePath(QStringLiteral("out.yaml"));
    QVERIFY(ClashConfigWriter::writeClientConfig(config, server, output, 12345).success);
    const QString firstRun = readFile(output);

    // Second run uses the same source YAML (not the patched output) and produces identical text.
    QVERIFY(ClashConfigWriter::writeClientConfig(config, server, output, 12345).success);
    const QString secondRun = readFile(output);

    QCOMPARE(firstRun, secondRun);
    QCOMPARE(firstRun.count(QStringLiteral("mixed-port:")), 1);
    QCOMPARE(firstRun.count(QStringLiteral("external-controller:")), 1);
}

void ClashConfigWriterTests::leavesNestedKeysUntouched()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // A tun: block that contains its own mixed-port / log-level child keys would be clobbered
    // by a naive top-level regex. Only the column-0 entries should change.
    const QString source = writeSampleYaml(dir, QStringLiteral(
        "mixed-port: 7890\n"
        "tun:\n"
        "  mixed-port: 9999\n"
        "  log-level: silent\n"
        "proxy-groups:\n"
        "  - name: default\n"
        "    proxies:\n"
        "      - auto\n"
        "proxies: []\n"));

    Config config;
    config.localPort = 10808;
    config.logLevel = QStringLiteral("warning");

    VmessItem server;
    server.coreType = CoreType::Clash;
    server.address = source;

    const QString output = QDir(dir.path()).filePath(QStringLiteral("out.yaml"));
    QVERIFY(ClashConfigWriter::writeClientConfig(config, server, output, 9090).success);

    const QString patched = readFile(output);
    QVERIFY(patched.contains(QStringLiteral("mixed-port: 10808")));
    QVERIFY(patched.contains(QStringLiteral("  mixed-port: 9999")));
    QVERIFY(patched.contains(QStringLiteral("log-level: warning")));
    QVERIFY(patched.contains(QStringLiteral("  log-level: silent")));
}

void ClashConfigWriterTests::preservesStrTagRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = writeSampleYaml(dir, QStringLiteral(
        "mixed-port: 7890\n"
        "proxies:\n"
        "  - name: sample\n"
        "    password: !<str> 1234567890\n"));

    Config config;
    config.localPort = 10808;
    VmessItem server;
    server.coreType = CoreType::Clash;
    server.address = source;

    const QString output = QDir(dir.path()).filePath(QStringLiteral("out.yaml"));
    QVERIFY(ClashConfigWriter::writeClientConfig(config, server, output, 9090).success);

    const QString patched = readFile(output);
    QVERIFY2(!patched.contains(QStringLiteral("!<str>")),
        "Raw !<str> tag must not remain; Clash parsers reject it.");
    QVERIFY(patched.contains(QStringLiteral("!!str")));
    QVERIFY(patched.contains(QStringLiteral("1234567890")));
}

QTEST_MAIN(ClashConfigWriterTests)

#include "ClashConfigWriterTests.moc"
