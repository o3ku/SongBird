#pragma once

#include <QString>
#include <QStringList>

#include "common/OperationResult.h"
#include "runtime/CoreInfo.h"

QStringList buildCoreConfigPreflightArguments(const CoreInfo& coreInfo, const QString& configFilePath);

OperationResult validateCoreConfigBeforeStart(
    const CoreInfo& coreInfo,
    const QString& configFilePath,
    int timeoutMs = 15000);
