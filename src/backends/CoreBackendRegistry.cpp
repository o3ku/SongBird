#include "runtime/core/CoreBackendRegistry.h"

#include <QFileInfo>

#include "backends/singbox/SingBoxCoreBackend.h"
#include "backends/xray/XrayCoreBackend.h"
#include "runtime/core/ICoreBackend.h"

namespace {

const XrayCoreBackend kXrayCoreBackend;
const SingBoxCoreBackend kSingBoxCoreBackend;

} // namespace

QList<const ICoreBackend*> coreBackends()
{
    return {
        &kXrayCoreBackend,
        &kSingBoxCoreBackend
    };
}

const ICoreBackend* coreBackend(CoreType coreType)
{
    const CoreType runtimeCoreType = resolveRuntimeCoreType(coreType);
    for (const ICoreBackend* backend : coreBackends()) {
        if (backend != nullptr && backend->type() == runtimeCoreType) {
            return backend;
        }
    }
    return nullptr;
}

const ICoreBackend* coreBackendForExecutableName(const QString& fileName)
{
    const QString normalizedFileName = QFileInfo(fileName).fileName().toLower();
    for (const ICoreBackend* backend : coreBackends()) {
        if (backend == nullptr) {
            continue;
        }
        for (const QString& executableName : backend->executableNames()) {
            const QString normalizedExecutableName = QFileInfo(executableName).fileName().toLower();
            if (normalizedFileName == normalizedExecutableName
                || normalizedFileName.contains(normalizedExecutableName.section(QChar('.'), 0, 0))) {
                return backend;
            }
        }
    }
    return nullptr;
}

QString normalizeCoreVersionTag(QString value)
{
    value = value.trimmed().toLower();
    if (value.startsWith(QStringLiteral("version "))) {
        value = value.mid(QStringLiteral("version ").size()).trimmed();
    }

    if (!value.isEmpty() && value.front().isDigit()) {
        value.prepend(QChar('v'));
    }

    return value;
}
