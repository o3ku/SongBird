#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <Qt>

#include "platform/windows/WindowsUwpLoopbackService.h"

class QTableWidget;

namespace UwpLoopbackDialogSupport {

constexpr int EnabledColumn = 0;
constexpr int AppColumn = 1;
constexpr int PackageColumn = 2;
constexpr int PackageRole = Qt::UserRole + 100;

void configurePackageTable(QTableWidget* table, const QStringList& headerLabels);
void populatePackageTable(QTableWidget* table, const QList<WindowsUwpPackageInfo>& packages);
void applyPackageFilter(QTableWidget* table, const QString& needle);
int enabledPackageCount(const QList<WindowsUwpPackageInfo>& packages);

} // namespace UwpLoopbackDialogSupport
