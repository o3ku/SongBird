#pragma once

#include <QDir>
#include <QList>
#include <QFileInfo>
#include <QString>
#include <QStringList>

#include <algorithm>

#include "runtime/core/CoreDescriptor.h"
#include "runtime/core/CoreDescriptorRegistry.h"

inline QList<CoreDescriptor> coreDescriptors()
{
    QList<CoreDescriptor> descriptors = registeredCoreDescriptors();
    std::sort(
        descriptors.begin(),
        descriptors.end(),
        [](const CoreDescriptor& left, const CoreDescriptor& right) {
            return static_cast<int>(left.type) < static_cast<int>(right.type);
        });
    return descriptors;
}

inline const CoreDescriptor* coreDescriptor(CoreType coreType)
{
    const CoreType runtimeCoreType = resolveRuntimeCoreType(coreType);
    static const QList<CoreDescriptor> descriptors = coreDescriptors();
    for (const CoreDescriptor& descriptor : descriptors) {
        if (descriptor.type == runtimeCoreType) {
            return &descriptor;
        }
    }
    return nullptr;
}

inline QList<CoreType> catalogCoreTypes()
{
    const QList<CoreDescriptor> descriptors = coreDescriptors();
    QList<CoreType> coreTypes;
    coreTypes.reserve(descriptors.size());
    for (const CoreDescriptor& descriptor : descriptors) {
        coreTypes.append(descriptor.type);
    }
    return coreTypes;
}

inline QStringList catalogCoreExecutableNames(CoreType coreType)
{
    const CoreDescriptor* descriptor = coreDescriptor(coreType);
    return descriptor != nullptr ? descriptor->executableNames : QStringList{};
}

inline QString catalogCoreDisplayName(CoreType coreType)
{
    const CoreDescriptor* descriptor = coreDescriptor(coreType);
    return descriptor != nullptr ? descriptor->displayName : QStringLiteral("Unknown");
}

inline QList<CoreType> sortedCatalogCoreTypes()
{
    QList<CoreDescriptor> descriptors = coreDescriptors();
    std::sort(
        descriptors.begin(),
        descriptors.end(),
        [](const CoreDescriptor& left, const CoreDescriptor& right) {
            return left.protocolPriority < right.protocolPriority;
        });

    QList<CoreType> coreTypes;
    coreTypes.reserve(descriptors.size());
    for (const CoreDescriptor& descriptor : descriptors) {
        coreTypes.append(descriptor.type);
    }
    return coreTypes;
}

inline CoreType catalogCoreTypeForExecutableName(const QString& fileName)
{
    const QString normalizedFileName = QFileInfo(fileName).fileName().toLower();
    for (const CoreDescriptor& descriptor : coreDescriptors()) {
        for (const QString& executableName : descriptor.executableNames) {
            const QString normalizedExecutableName = QFileInfo(executableName).fileName().toLower();
            if (normalizedFileName == normalizedExecutableName
                || normalizedFileName.contains(normalizedExecutableName.section(QChar('.'), 0, 0))) {
                return descriptor.type;
            }
        }
    }
    return CoreType::Unknown;
}

inline QList<CoreType> catalogAuxiliaryTunCoreTypes(CoreType coreType)
{
    const CoreDescriptor* descriptor = coreDescriptor(coreType);
    return descriptor != nullptr ? descriptor->auxiliaryTunCoreTypes : QList<CoreType>{};
}

inline QList<CoreGeoFileRequirement> catalogCoreGeoFileRequirements(CoreType coreType)
{
    const CoreDescriptor* descriptor = coreDescriptor(coreType);
    return descriptor != nullptr ? descriptor->geoFileRequirements : QList<CoreGeoFileRequirement>{};
}

inline bool catalogCoreNeedsGeoFiles(CoreType coreType)
{
    return !catalogCoreGeoFileRequirements(coreType).isEmpty();
}

inline QString catalogCoreDataDirectoryName(CoreType coreType)
{
    const CoreDescriptor* descriptor = coreDescriptor(coreType);
    return descriptor != nullptr ? descriptor->dataDirectoryName : QString();
}

// Absolute path of the core's own data directory, or empty when it declares none -- in which case
// the core reads its data from the directory it runs in. The application directory is a parameter
// rather than read from QCoreApplication so the resolution stays testable against a temp dir.
inline QString catalogCoreDataDirectory(CoreType coreType, const QString& applicationDirectory)
{
    const QString dataDirectoryName = catalogCoreDataDirectoryName(coreType).trimmed();
    return dataDirectoryName.isEmpty()
        ? QString()
        : QDir(applicationDirectory).filePath(dataDirectoryName);
}
