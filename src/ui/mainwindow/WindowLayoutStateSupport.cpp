#include "ui/mainwindow/WindowLayoutStateSupport.h"

#include <algorithm>

#include <QGuiApplication>
#include <QHeaderView>
#include <QScreen>
#include <QSplitter>

namespace {

constexpr int ManualServerColumnWidthsMarker = 1;

} // namespace

namespace WindowLayoutStateSupport {

QList<int> defaultServerColumnWidths()
{
    return {
        44,
        90,
        220,
        320,
        84
    };
}

QStringList serverColumnKeys()
{
    return {
        QStringLiteral("Default"),
        QStringLiteral("Type"),
        QStringLiteral("Remarks"),
        QStringLiteral("Address"),
        QStringLiteral("TestResult")
    };
}

QString manualServerColumnWidthsKey()
{
    return QStringLiteral("__manualServerColumnWidths");
}

bool hasLegacyServerColumnWidths(const QMap<QString, int>& widths)
{
    return widths.contains(QStringLiteral("Port"));
}

bool hasManualServerColumnWidths(const QMap<QString, int>& widths)
{
    return widths.value(manualServerColumnWidthsKey()) == ManualServerColumnWidthsMarker;
}

void restoreServerColumnWidths(QHeaderView* header, const QMap<QString, int>& widths)
{
    if (header == nullptr || hasLegacyServerColumnWidths(widths) || !hasManualServerColumnWidths(widths)) {
        return;
    }

    QMap<QString, int> effectiveWidths = widths;
    effectiveWidths.remove(manualServerColumnWidthsKey());
    const QStringList keys = serverColumnKeys();
    for (int index = 0; index < keys.size() && index < header->count(); ++index) {
        const auto it = effectiveWidths.constFind(keys.at(index));
        if (it != effectiveWidths.constEnd() && it.value() > 0) {
            header->resizeSection(index, it.value());
        }
    }
}

QMap<QString, int> captureServerColumnWidths(const QHeaderView* header)
{
    QMap<QString, int> widths;
    if (header == nullptr) {
        return widths;
    }

    const QStringList keys = serverColumnKeys();
    for (int index = 0; index < keys.size() && index < header->count(); ++index) {
        if (index == ServerResultColumn) {
            continue;
        }
        widths.insert(keys.at(index), header->sectionSize(index));
    }
    widths.insert(manualServerColumnWidthsKey(), ManualServerColumnWidthsMarker);

    return widths;
}

int clampSplitPercent(int value, int fallback)
{
    return value < 10 || value > 90 ? fallback : value;
}

int captureSplitPercent(QSplitter* splitter, int fallback)
{
    if (splitter == nullptr) {
        return clampSplitPercent(fallback, fallback);
    }

    const QList<int> sizes = splitter->sizes();
    if (sizes.size() < 2) {
        return clampSplitPercent(fallback, fallback);
    }

    const int total = qMax(0, sizes.at(0)) + qMax(0, sizes.at(1));
    if (total <= 0) {
        return clampSplitPercent(fallback, fallback);
    }

    const int percent = static_cast<int>(qRound((sizes.at(0) * 100.0) / total));
    return clampSplitPercent(percent, fallback);
}

void applySplitPercent(QSplitter* splitter, int percent, int fallback)
{
    if (splitter == nullptr) {
        return;
    }

    const QList<int> sizes = splitter->sizes();
    if (sizes.size() < 2) {
        return;
    }

    const int normalizedPercent = clampSplitPercent(percent, fallback);
    int total = qMax(0, sizes.at(0)) + qMax(0, sizes.at(1));
    if (total <= 0) {
        total = splitter->orientation() == Qt::Horizontal
            ? splitter->contentsRect().width()
            : splitter->contentsRect().height();
    }

    if (total <= 0) {
        return;
    }

    const int firstSize = qMax(1, static_cast<int>(qRound((total * normalizedPercent) / 100.0)));
    const int secondSize = qMax(1, total - firstSize);
    splitter->setSizes({firstSize, secondSize});
}

QPoint clampWindowPosition(const QPoint& topLeft, const QSize& size)
{
    QList<QRect> workingAreas;
    const auto screens = QGuiApplication::screens();
    for (QScreen* screen : screens) {
        if (screen != nullptr) {
            workingAreas.append(screen->availableGeometry());
        }
    }

    QRect targetArea;
    for (const QRect& area : workingAreas) {
        if (area.contains(topLeft) || area.intersects(QRect(topLeft, size))) {
            targetArea = area;
            break;
        }
    }

    if (!targetArea.isValid()) {
        if (QScreen* primaryScreen = QGuiApplication::primaryScreen()) {
            targetArea = primaryScreen->availableGeometry();
        }
    }

    if (!targetArea.isValid()) {
        return topLeft;
    }

    const int maxX = std::max(targetArea.left(), targetArea.right() - size.width() + 1);
    const int maxY = std::max(targetArea.top(), targetArea.bottom() - size.height() + 1);
    return QPoint(
        qBound(targetArea.left(), topLeft.x(), maxX),
        qBound(targetArea.top(), topLeft.y(), maxY));
}

} // namespace WindowLayoutStateSupport
