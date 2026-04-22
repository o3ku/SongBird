#include "runtime/ClashConfigWriter.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringList>
#include <QTextStream>

namespace {

const QString kStrTagMarker = QStringLiteral("!<str>");
const QString kStrTagPlaceholder = QStringLiteral("__strn__");
const QString kStrTagCanonical = QStringLiteral("!!str");

QString resolveSourceYamlPath(const QString& addressField)
{
    const QString trimmed = addressField.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    if (QFileInfo::exists(trimmed)) {
        return trimmed;
    }

    const QString appRelative = QDir(QCoreApplication::applicationDirPath()).filePath(trimmed);
    if (QFileInfo::exists(appRelative)) {
        return appRelative;
    }

    return trimmed;
}

QString mapLogLevel(const QString& level)
{
    const QString normalized = level.trimmed().toLower();
    if (normalized == QStringLiteral("none")) {
        return QStringLiteral("silent");
    }
    if (normalized.isEmpty()) {
        return QStringLiteral("warning");
    }
    return normalized;
}

// Detect the column-0 leading character of `line`. Top-level YAML keys have no
// indentation; anything with leading whitespace belongs to a nested mapping or
// sequence item.
bool isTopLevelKeyLine(const QString& line, const QString& key)
{
    if (!line.startsWith(key)) {
        return false;
    }
    const int keyEnd = key.size();
    if (keyEnd >= line.size()) {
        return false;
    }
    const QChar next = line.at(keyEnd);
    return next == QChar(':') || next == QChar(' ') || next == QChar('\t');
}

} // namespace

OperationResult ClashConfigWriter::writeClientConfig(
    const Config& config,
    const VmessItem& server,
    const QString& outputPath,
    int controllerPort)
{
    const QString sourcePath = resolveSourceYamlPath(server.address);
    if (sourcePath.isEmpty() || !QFileInfo::exists(sourcePath)) {
        return OperationResult::fail(QStringLiteral(
            "Clash subscription YAML not found. The server Address field must point to a local YAML file."));
    }

    QFile source(sourcePath);
    if (!source.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return OperationResult::fail(QStringLiteral("Failed to read Clash YAML: %1").arg(sourcePath));
    }
    const QString rawText = QString::fromUtf8(source.readAll());
    source.close();

    const QString patched = patchYamlContent(rawText, config, controllerPort);

    const QString outputDir = QFileInfo(outputPath).absolutePath();
    if (!QDir().mkpath(outputDir)) {
        return OperationResult::fail(QStringLiteral("Failed to create runtime directory: %1").arg(outputDir));
    }

    QSaveFile output(outputPath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return OperationResult::fail(QStringLiteral("Failed to open runtime YAML for write: %1").arg(outputPath));
    }
    if (output.write(patched.toUtf8()) < 0) {
        return OperationResult::fail(QStringLiteral("Failed to write runtime Clash YAML."));
    }
    if (!output.commit()) {
        return OperationResult::fail(QStringLiteral("Failed to commit runtime Clash YAML."));
    }

    return OperationResult::ok(
        QStringLiteral("Clash runtime YAML generated: %1").arg(QDir::toNativeSeparators(outputPath)));
}

QString ClashConfigWriter::patchYamlContent(const QString& source, const Config& config, int controllerPort)
{
    // YAML parsers reject "!<str>" in unquoted keys; the reference implementation round-trips it
    // via a placeholder before/after editing. We do the same so Clash cores accept our output.
    QString text = source;
    text.replace(kStrTagMarker, kStrTagPlaceholder);

    replaceOrAppend(text, QStringLiteral("mixed-port"), QString::number(config.localPort));
    replaceOrAppend(text, QStringLiteral("allow-lan"), formatBool(config.allowLanConnection));
    if (config.allowLanConnection) {
        replaceOrAppend(text, QStringLiteral("bind-address"), QStringLiteral("\"*\""));
    } else {
        removeTopLevelKey(text, QStringLiteral("bind-address"));
    }
    if (controllerPort > 0) {
        replaceOrAppend(
            text,
            QStringLiteral("external-controller"),
            QStringLiteral("\"127.0.0.1:%1\"").arg(controllerPort));
    }
    replaceOrAppend(text, QStringLiteral("log-level"), mapLogLevel(config.logLevel));
    replaceOrAppend(text, QStringLiteral("ipv6"), formatBool(false));

    // Strip any top-level `secret` so we can hit the REST API without a token.
    removeTopLevelKey(text, QStringLiteral("secret"));

    text.replace(kStrTagPlaceholder, kStrTagCanonical);
    return text;
}

bool ClashConfigWriter::replaceOrAppend(QString& text, const QString& key, const QString& valueLine)
{
    QStringList lines = text.split(QChar('\n'));
    const QString formattedValue = QStringLiteral("%1: %2").arg(key, valueLine);

    bool replaced = false;
    for (int i = 0; i < lines.size(); ++i) {
        if (isTopLevelKeyLine(lines.at(i), key)) {
            lines[i] = formattedValue;
            replaced = true;
            break;
        }
    }

    if (replaced) {
        text = lines.join(QChar('\n'));
        return true;
    }

    if (!text.isEmpty() && !text.endsWith(QChar('\n'))) {
        text.append(QChar('\n'));
    }
    text.append(formattedValue);
    text.append(QChar('\n'));
    return false;
}

void ClashConfigWriter::removeTopLevelKey(QString& text, const QString& key)
{
    QStringList lines = text.split(QChar('\n'));
    for (int i = 0; i < lines.size();) {
        if (isTopLevelKeyLine(lines.at(i), key)) {
            lines.removeAt(i);
            continue;
        }
        ++i;
    }
    text = lines.join(QChar('\n'));
}

QString ClashConfigWriter::formatBool(bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

QString ClashConfigWriter::formatScalar(const QString& value)
{
    return value;
}
