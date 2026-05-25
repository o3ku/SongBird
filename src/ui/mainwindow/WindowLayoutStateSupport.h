#pragma once

#include <QList>
#include <QMap>
#include <QPoint>
#include <QSize>
#include <QStringList>

class QHeaderView;
class QSplitter;

namespace WindowLayoutStateSupport {

constexpr int DefaultServerLogSplitPercent = 60;
constexpr int DefaultServerQrSplitPercent = 78;
constexpr int DefaultMainWindowWidth = 1000;
constexpr int DefaultMainWindowHeight = 640;
constexpr int ServerResultColumn = 4;

QList<int> defaultServerColumnWidths();
QStringList serverColumnKeys();
QString manualServerColumnWidthsKey();
bool hasLegacyServerColumnWidths(const QMap<QString, int>& widths);
bool hasManualServerColumnWidths(const QMap<QString, int>& widths);
void restoreServerColumnWidths(QHeaderView* header, const QMap<QString, int>& widths);
QMap<QString, int> captureServerColumnWidths(const QHeaderView* header);
int clampSplitPercent(int value, int fallback);
int captureSplitPercent(QSplitter* splitter, int fallback);
void applySplitPercent(QSplitter* splitter, int percent, int fallback);
QPoint clampWindowPosition(const QPoint& topLeft, const QSize& size);

} // namespace WindowLayoutStateSupport
