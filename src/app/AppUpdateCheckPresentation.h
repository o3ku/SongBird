#pragma once

#include <QString>

#include "common/OperationResult.h"
#include "services/AppUpdateService.h"

namespace AppUpdateCheckPresentation {

QString buildUpdateAvailablePrompt(const AppUpdateCheckResult& updateResult, const QString& resultMessage);
QString appendMissingDownloadPrompt(const QString& prompt);
QString resolveDownloadAssetName(const QString& assetName);
QString buildDownloadedPackageMessage(const QString& savedPath, const OperationResult& result);

} // namespace AppUpdateCheckPresentation
