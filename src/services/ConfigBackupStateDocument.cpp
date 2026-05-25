#include "services/ConfigBackupStateDocument.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStringList>
#include <QJsonValue>

QString ConfigBackupStateDocument::stateConfigPathFor(const QString& configPath)
{
    const QFileInfo fileInfo(configPath);
    const QString baseName = fileInfo.completeBaseName().trimmed().isEmpty()
        ? QStringLiteral("songbird")
        : fileInfo.completeBaseName();
    return fileInfo.dir().filePath(QStringLiteral("%1.state.json").arg(baseName));
}

QJsonObject ConfigBackupStateDocument::readJsonObject(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object() : QJsonObject();
}

bool ConfigBackupStateDocument::writeJsonObject(const QString& path, const QJsonObject& root)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    if (file.write(QJsonDocument(root).toJson(QJsonDocument::Compact)) < 0) {
        return false;
    }

    return file.commit();
}

void ConfigBackupStateDocument::mergeStateIntoPrimary(QJsonObject& primaryRoot, const QJsonObject& stateRoot)
{
    if (stateRoot.contains(QStringLiteral("ui")) && stateRoot.value(QStringLiteral("ui")).isObject()) {
        QJsonObject ui = primaryRoot.value(QStringLiteral("ui")).toObject();
        const QJsonObject stateUi = stateRoot.value(QStringLiteral("ui")).toObject();
        for (auto it = stateUi.constBegin(); it != stateUi.constEnd(); ++it) {
            ui.insert(it.key(), it.value());
        }
        if (!ui.isEmpty()) {
            primaryRoot.insert(QStringLiteral("ui"), ui);
        }
    }

    const QJsonArray serverStates = stateRoot.value(QStringLiteral("serverStates")).toArray();
    if (serverStates.isEmpty()) {
        return;
    }

    QJsonArray servers = primaryRoot.value(QStringLiteral("servers")).toArray();
    for (int i = 0; i < servers.size(); ++i) {
        if (!servers.at(i).isObject()) {
            continue;
        }

        QJsonObject server = servers.at(i).toObject();
        const QString indexId = server.value(QStringLiteral("indexId")).toString();
        if (indexId.trimmed().isEmpty()) {
            continue;
        }

        for (const QJsonValue& value : serverStates) {
            if (!value.isObject()) {
                continue;
            }

            const QJsonObject stateObject = value.toObject();
            if (stateObject.value(QStringLiteral("indexId")).toString() != indexId) {
                continue;
            }

            if (stateObject.contains(QStringLiteral("testResult"))) {
                server.insert(QStringLiteral("testResult"), stateObject.value(QStringLiteral("testResult")));
            }
            servers[i] = server;
            break;
        }
    }

    if (!servers.isEmpty()) {
        primaryRoot.insert(QStringLiteral("servers"), servers);
    }
}

QJsonObject ConfigBackupStateDocument::extractStateFromMergedRoot(QJsonObject& primaryRoot)
{
    QJsonObject stateRoot;

    if (primaryRoot.contains(QStringLiteral("ui")) && primaryRoot.value(QStringLiteral("ui")).isObject()) {
        QJsonObject ui = primaryRoot.value(QStringLiteral("ui")).toObject();
        QJsonObject stateUi;
        const QStringList stateKeys = {
            QStringLiteral("mainLocationX"),
            QStringLiteral("mainLocationY"),
            QStringLiteral("mainSizeWidth"),
            QStringLiteral("mainSizeHeight"),
            QStringLiteral("mainServerSortColumn"),
            QStringLiteral("mainServerSortOrder"),
            QStringLiteral("mainSelectedSubscriptionId"),
            QStringLiteral("settingsRoutingRuleTabKey"),
            QStringLiteral("mainServerLogSplitPercent"),
            QStringLiteral("mainServerQrSplitPercent"),
            QStringLiteral("mainQrPreviewVisible"),
            QStringLiteral("mainProxyEnabled"),
            QStringLiteral("mainColumnWidths")
        };
        for (const QString& key : stateKeys) {
            if (ui.contains(key)) {
                stateUi.insert(key, ui.take(key));
            }
        }
        if (!stateUi.isEmpty()) {
            stateRoot.insert(QStringLiteral("ui"), stateUi);
        }
        if (ui.isEmpty()) {
            primaryRoot.remove(QStringLiteral("ui"));
        } else {
            primaryRoot.insert(QStringLiteral("ui"), ui);
        }
    }

    if (primaryRoot.contains(QStringLiteral("servers")) && primaryRoot.value(QStringLiteral("servers")).isArray()) {
        QJsonArray servers = primaryRoot.value(QStringLiteral("servers")).toArray();
        QJsonArray serverStates;
        for (int i = 0; i < servers.size(); ++i) {
            if (!servers.at(i).isObject()) {
                continue;
            }

            QJsonObject server = servers.at(i).toObject();
            QJsonObject stateObject;
            const QString indexId = server.value(QStringLiteral("indexId")).toString();
            if (!indexId.trimmed().isEmpty()) {
                stateObject.insert(QStringLiteral("indexId"), indexId);
            }
            if (server.contains(QStringLiteral("testResult"))) {
                stateObject.insert(QStringLiteral("testResult"), server.take(QStringLiteral("testResult")));
            }
            if (stateObject.size() > 1) {
                serverStates.append(stateObject);
            }
            servers[i] = server;
        }
        primaryRoot.insert(QStringLiteral("servers"), servers);
        if (!serverStates.isEmpty()) {
            stateRoot.insert(QStringLiteral("serverStates"), serverStates);
        }
    }

    return stateRoot;
}
