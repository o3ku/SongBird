#include "platform/windows/WindowsAutoRunService.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>

bool WindowsAutoRunService::isEnabled() const
{
    QSettings settings(QString::fromUtf8(AutoRunRegistryPath), QSettings::NativeFormat);
    const QString current = settings.value(QString::fromUtf8(AutoRunValueName)).toString().trimmed();
    const QString expected = QStringLiteral("\"%1\"").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
    return current.compare(expected, Qt::CaseInsensitive) == 0
        || current.compare(QDir::toNativeSeparators(QCoreApplication::applicationFilePath()), Qt::CaseInsensitive) == 0;
}

bool WindowsAutoRunService::setEnabled(bool enabled) const
{
    QSettings settings(QString::fromUtf8(AutoRunRegistryPath), QSettings::NativeFormat);
    if (enabled) {
        settings.setValue(
            QString::fromUtf8(AutoRunValueName),
            QStringLiteral("\"%1\"").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath())));
    } else {
        settings.remove(QString::fromUtf8(AutoRunValueName));
    }

    settings.sync();
    return settings.status() == QSettings::NoError;
}
