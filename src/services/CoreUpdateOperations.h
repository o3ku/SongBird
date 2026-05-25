#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "common/OperationResult.h"
#include "services/CoreUpdateService.h"

namespace CoreUpdateOperations {

void reportProgress(const CoreUpdateService::ProgressHandler& progressHandler, const QString& message);
bool isCancellationRequested(const CoreUpdateService::CancelCheckHandler& cancelCheck);
OperationResult cancelledResult();
bool isCancelledResult(const OperationResult& result);

OperationResult downloadBytesWithNetwork(
    const QUrl& url,
    QByteArray* content,
    const QString& userAgent,
    int timeoutMs,
    const CoreUpdateService::CancelCheckHandler& cancelCheck,
    const CoreUpdateService::ProgressHandler& progressHandler = {});

OperationResult runVersionCommandWithProcess(
    const QString& program,
    const QStringList& arguments,
    QString* output,
    const CoreUpdateService::CancelCheckHandler& cancelCheck);

OperationResult extractArchiveWithPowerShell(
    const QString& archivePath,
    const QString& extractionDirectory,
    const CoreUpdateService::CancelCheckHandler& cancelCheck);

} // namespace CoreUpdateOperations
