#include "services/ServerCustomConfigStore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>

#include <utility>

ServerCustomConfigStore::ServerCustomConfigStore(QString customConfigDirectory)
    : customConfigDirectory_(std::move(customConfigDirectory))
{
}

QString ServerCustomConfigStore::resolveConfigPath(const QString& address) const
{
    const QString trimmed = address.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    if (QFileInfo::exists(trimmed)) {
        return QFileInfo(trimmed).absoluteFilePath();
    }

    if (!customConfigDirectory_.trimmed().isEmpty()) {
        const QString managedPath = QDir(customConfigDirectory_).filePath(trimmed);
        if (QFileInfo::exists(managedPath)) {
            return QFileInfo(managedPath).absoluteFilePath();
        }
    }

    return trimmed;
}

OperationResult ServerCustomConfigStore::prepareServer(VmessItem& server, const VmessItem* existing) const
{
    const QString sourcePath = resolveConfigPath(server.address);
    if (sourcePath.trimmed().isEmpty() || !QFileInfo::exists(sourcePath)) {
        return OperationResult::fail(QCoreApplication::translate("ServerCustomConfigStore", "Custom config file does not exist."));
    }

    if (customConfigDirectory_.trimmed().isEmpty()) {
        return OperationResult::fail(QCoreApplication::translate("ServerCustomConfigStore", "Custom config directory is unavailable."));
    }

    QDir customDirectory(customConfigDirectory_);
    if (!customDirectory.exists() && !QDir().mkpath(customDirectory.absolutePath())) {
        return OperationResult::fail(QCoreApplication::translate("ServerCustomConfigStore", "Failed to create custom config directory."));
    }

    const QString existingStoredPath = existing == nullptr ? QString() : resolveConfigPath(existing->address);
    if (existing != nullptr
        && !existingStoredPath.isEmpty()
        && QFileInfo(existingStoredPath).absoluteFilePath().compare(QFileInfo(sourcePath).absoluteFilePath(), Qt::CaseInsensitive) == 0) {
        server.address = existing->address;
        return OperationResult::ok();
    }

    const QString extension = QFileInfo(sourcePath).suffix();
    const QString targetFileName = extension.isEmpty()
        ? QUuid::createUuid().toString(QUuid::WithoutBraces)
        : QStringLiteral("%1.%2").arg(QUuid::createUuid().toString(QUuid::WithoutBraces), extension);
    const QString targetPath = customDirectory.filePath(targetFileName);

    if (QFileInfo::exists(targetPath)) {
        QFile::remove(targetPath);
    }
    if (!QFile::copy(sourcePath, targetPath)) {
        return OperationResult::fail(QCoreApplication::translate("ServerCustomConfigStore", "Failed to copy custom config file into managed storage."));
    }

    if (!existingStoredPath.isEmpty() && isManagedConfigPath(existingStoredPath) && QFileInfo::exists(existingStoredPath)) {
        QFile::remove(existingStoredPath);
    }

    server.address = targetFileName;
    server.port = 0;
    return OperationResult::ok();
}

OperationResult ServerCustomConfigStore::removeManagedConfig(const QString& address) const
{
    const QString resolvedPath = resolveConfigPath(address);
    if (!isManagedConfigPath(resolvedPath) || !QFileInfo::exists(resolvedPath)) {
        // Nothing of ours to delete: the address is not managed storage, or the file is already
        // gone. Reporting this as a failure would turn the ordinary "no file to clean up" case
        // into a spurious error.
        return OperationResult::ok();
    }

    // QFile::remove() is static and cannot report why it failed, so use an instance to keep
    // errorString() for the message.
    QFile file(resolvedPath);
    if (file.remove()) {
        return OperationResult::ok();
    }

    return OperationResult::fail(
        QCoreApplication::translate("ServerCustomConfigStore", "Failed to delete the managed custom config file %1: %2")
            .arg(QDir::toNativeSeparators(resolvedPath), file.errorString()));
}

bool ServerCustomConfigStore::isManagedConfigPath(const QString& filePath) const
{
    if (customConfigDirectory_.trimmed().isEmpty() || filePath.trimmed().isEmpty()) {
        return false;
    }

    const QString rootPath = QDir::cleanPath(QDir(customConfigDirectory_).absolutePath()).replace('\\', '/').toLower();
    const QString absoluteFilePath = QDir::cleanPath(QFileInfo(filePath).absoluteFilePath()).replace('\\', '/').toLower();
    return absoluteFilePath == rootPath
        || absoluteFilePath.startsWith(rootPath + QStringLiteral("/"));
}
