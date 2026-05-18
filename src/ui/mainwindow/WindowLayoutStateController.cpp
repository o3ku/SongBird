#include "ui/mainwindow/WindowLayoutStateController.h"

#include <QGuiApplication>
#include <QHeaderView>
#include <QMainWindow>
#include <QPoint>
#include <QScreen>
#include <QSplitter>
#include <QTableView>
#include <QTimer>

#include <numeric>

#include "ui/mainwindow/ServerTableView.h"
#include "ui/mainwindow/ServerWorkspaceWidget.h"
#include "ui/mainwindow/SharePanelWidget.h"

namespace {

constexpr int DefaultServerLogSplitPercent = 60;
constexpr int DefaultServerQrSplitPercent = 78;
constexpr int DefaultMainWindowWidth = 1000;
constexpr int DefaultMainWindowHeight = 640;

QStringList defaultServerColumnKeys()
{
    return {
        QStringLiteral("Default"),
        QStringLiteral("Type"),
        QStringLiteral("Remarks"),
        QStringLiteral("Address"),
        QStringLiteral("Security"),
        QStringLiteral("Network"),
        QStringLiteral("StreamSecurity"),
        QStringLiteral("TestResult")
    };
}

bool hasLegacyServerColumnWidths(const QMap<QString, int>& widths)
{
    return widths.contains(QStringLiteral("Port"));
}

} // namespace

WindowLayoutStateController::WindowLayoutStateController(
    QMainWindow* window,
    ServerWorkspaceWidget* serverWorkspace,
    ServerTableView* serverView,
    SharePanelWidget* sharePanel)
    : QObject(window)
    , window_(window)
    , serverWorkspace_(serverWorkspace)
    , serverView_(serverView)
    , sharePanel_(sharePanel)
{
    if (sharePanel_ != nullptr) {
        sharePanel_->setPreviewVisible(qrPreviewVisible_);
    }
}

QList<int> WindowLayoutStateController::defaultServerColumnWidths(int availableWidth)
{
    QList<int> widths{
        44,
        100,
        160,
        240,
        80,
        80,
        64,
        84
    };

    const int minimumTotalWidth = std::accumulate(widths.cbegin(), widths.cend(), 0);
    if (availableWidth <= 0) {
        availableWidth = DefaultMainWindowWidth;
    }
    if (availableWidth <= minimumTotalWidth) {
        return widths;
    }

    const int extraWidth = availableWidth - minimumTotalWidth;
    const int aliasExtraWidth = qRound((extraWidth * 3.0) / 7.0);
    widths[2] += aliasExtraWidth;
    widths[3] += extraWidth - aliasExtraWidth;
    return widths;
}

bool WindowLayoutStateController::qrPreviewVisible() const
{
    return qrPreviewVisible_;
}

void WindowLayoutStateController::restoreFromConfig(const Config& config)
{
    if (window_ == nullptr) {
        return;
    }

    const QSize restoredSize(
        qMax(window_->minimumWidth(), config.mainSizeWidth > 0 ? config.mainSizeWidth : window_->width()),
        qMax(window_->minimumHeight(), config.mainSizeHeight > 0 ? config.mainSizeHeight : window_->height()));
    if (restoredSize.width() > 0 && restoredSize.height() > 0) {
        window_->resize(restoredSize);
    }

    if (config.mainLocationX != 0 || config.mainLocationY != 0) {
        window_->move(clampWindowPosition(QPoint(config.mainLocationX, config.mainLocationY), window_->size()));
    }

    pendingColumnWidths_ = config.mainColumnWidths;
    serverLogSplitPercent_ = clampSplitPercent(config.mainServerLogSplitPercent, DefaultServerLogSplitPercent);
    serverQrSplitPercent_ = clampSplitPercent(config.mainServerQrSplitPercent, DefaultServerQrSplitPercent);
    qrPreviewVisible_ = config.mainQrPreviewVisible;
    if (sharePanel_ != nullptr) {
        sharePanel_->setPreviewVisible(qrPreviewVisible_);
    }

    uiStateRestorePending_ = true;
}

void WindowLayoutStateController::captureToConfig(Config& config) const
{
    if (window_ == nullptr) {
        return;
    }

    QRect bounds = window_->geometry();
    const QRect normalBounds = window_->normalGeometry();
    if ((window_->isMaximized() || window_->isMinimized()) && normalBounds.isValid()) {
        bounds = normalBounds;
    }

    config.mainLocationX = bounds.x();
    config.mainLocationY = bounds.y();
    config.mainSizeWidth = bounds.width();
    config.mainSizeHeight = bounds.height();
    config.mainColumnWidths = captureServerColumnWidths();
    config.mainServerLogSplitPercent = captureSplitPercent(
        serverWorkspace_ == nullptr ? nullptr : serverWorkspace_->rootSplitter(),
        serverLogSplitPercent_);
    config.mainServerQrSplitPercent = captureSplitPercent(
        serverWorkspace_ == nullptr ? nullptr : serverWorkspace_->topSplitter(),
        serverQrSplitPercent_);
}

void WindowLayoutStateController::handleShowEvent()
{
    if (window_ == nullptr) {
        return;
    }

    applyFrameAdjustedWindowMetrics();
    if (uiStateRestorePending_) {
        uiStateRestorePending_ = false;
        initialServerColumnLayoutPending_ = false;
        QTimer::singleShot(0, this, [this]() {
            applyDeferredUiState();
        });
        return;
    }

    if (!initialServerColumnLayoutPending_) {
        return;
    }

    initialServerColumnLayoutPending_ = false;
    QTimer::singleShot(0, this, [this]() {
        restoreServerColumnWidths({});
    });
}

void WindowLayoutStateController::handleQrPreviewToggled(bool checked)
{
    if (qrPreviewVisible_ && !checked) {
        serverQrSplitPercent_ = captureSplitPercent(
            serverWorkspace_ == nullptr ? nullptr : serverWorkspace_->topSplitter(),
            serverQrSplitPercent_);
    }

    qrPreviewVisible_ = checked;
    if (sharePanel_ != nullptr) {
        sharePanel_->setPreviewVisible(qrPreviewVisible_);
    }

    if (qrPreviewVisible_) {
        QTimer::singleShot(0, this, [this]() {
            applySplitPercent(
                serverWorkspace_ == nullptr ? nullptr : serverWorkspace_->topSplitter(),
                serverQrSplitPercent_);
        });
    }
}

void WindowLayoutStateController::handleTopSplitterMoved()
{
    if (serverWorkspace_ == nullptr) {
        return;
    }

    serverQrSplitPercent_ = captureSplitPercent(serverWorkspace_->topSplitter(), serverQrSplitPercent_);
}

void WindowLayoutStateController::handleRootSplitterMoved()
{
    if (serverWorkspace_ == nullptr) {
        return;
    }

    serverLogSplitPercent_ = captureSplitPercent(serverWorkspace_->rootSplitter(), serverLogSplitPercent_);
}

void WindowLayoutStateController::applyDeferredUiState()
{
    restoreServerColumnWidths(pendingColumnWidths_);
    pendingColumnWidths_.clear();

    if (sharePanel_ != nullptr && qrPreviewVisible_) {
        sharePanel_->setPreviewVisible(true);
    }
    applySplitPercent(
        serverWorkspace_ == nullptr ? nullptr : serverWorkspace_->topSplitter(),
        serverQrSplitPercent_);
    if (sharePanel_ != nullptr) {
        sharePanel_->setPreviewVisible(qrPreviewVisible_);
    }

    applySplitPercent(
        serverWorkspace_ == nullptr ? nullptr : serverWorkspace_->rootSplitter(),
        serverLogSplitPercent_);
}

void WindowLayoutStateController::applyFrameAdjustedWindowMetrics()
{
    if (window_ == nullptr || frameAdjustedWindowMetricsApplied_) {
        return;
    }

    const QSize clientSize = window_->size();
    const QSize frameSize = window_->frameGeometry().size();
    if (!clientSize.isValid() || !frameSize.isValid()) {
        return;
    }

    const QSize frameDelta(
        qMax(0, frameSize.width() - clientSize.width()),
        qMax(0, frameSize.height() - clientSize.height()));
    const QSize targetClientSize(
        qMax(1, DefaultMainWindowWidth - frameDelta.width()),
        qMax(1, DefaultMainWindowHeight - frameDelta.height()));

    frameAdjustedWindowMetricsApplied_ = true;
    window_->setMinimumSize(targetClientSize);

    if (clientSize == QSize(DefaultMainWindowWidth, DefaultMainWindowHeight)) {
        window_->resize(targetClientSize);
    }
}

void WindowLayoutStateController::restoreServerColumnWidths(const QMap<QString, int>& widths)
{
    if (serverView_ == nullptr || serverView_->horizontalHeader() == nullptr) {
        return;
    }

    const QMap<QString, int> effectiveWidths = hasLegacyServerColumnWidths(widths)
        ? QMap<QString, int>()
        : widths;
    const int availableWidth = serverView_->viewport() == nullptr
        ? serverView_->width()
        : serverView_->viewport()->width();
    const QList<int> defaultWidths = defaultServerColumnWidths(availableWidth);
    const QStringList keys = serverColumnKeys();
    QHeaderView* header = serverView_->horizontalHeader();
    for (int index = 0; index < keys.size() && index < header->count(); ++index) {
        if (index < defaultWidths.size() && defaultWidths.at(index) > 0) {
            header->resizeSection(index, defaultWidths.at(index));
        }
        const auto it = effectiveWidths.constFind(keys.at(index));
        if (it != effectiveWidths.constEnd() && it.value() > 0) {
            header->resizeSection(index, it.value());
        }
    }
}

QMap<QString, int> WindowLayoutStateController::captureServerColumnWidths() const
{
    QMap<QString, int> widths;
    if (serverView_ == nullptr || serverView_->horizontalHeader() == nullptr) {
        return widths;
    }

    const QStringList keys = serverColumnKeys();
    const QHeaderView* header = serverView_->horizontalHeader();
    for (int index = 0; index < keys.size() && index < header->count(); ++index) {
        widths.insert(keys.at(index), header->sectionSize(index));
    }

    return widths;
}

int WindowLayoutStateController::captureSplitPercent(QSplitter* splitter, int fallback) const
{
    if (splitter == nullptr) {
        return clampSplitPercent(fallback, fallback);
    }

    if (serverWorkspace_ != nullptr
        && splitter == serverWorkspace_->topSplitter()
        && (sharePanel_ == nullptr || !sharePanel_->isVisible())) {
        return clampSplitPercent(serverQrSplitPercent_, fallback);
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

void WindowLayoutStateController::applySplitPercent(QSplitter* splitter, int percent)
{
    if (splitter == nullptr) {
        return;
    }

    const QList<int> sizes = splitter->sizes();
    if (sizes.size() < 2) {
        return;
    }

    const int normalizedPercent = clampSplitPercent(
        percent,
        serverWorkspace_ != nullptr && splitter == serverWorkspace_->rootSplitter()
            ? DefaultServerLogSplitPercent
            : DefaultServerQrSplitPercent);
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

int WindowLayoutStateController::clampSplitPercent(int value, int fallback)
{
    return value < 10 || value > 90 ? fallback : value;
}

QStringList WindowLayoutStateController::serverColumnKeys()
{
    return defaultServerColumnKeys();
}

QPoint WindowLayoutStateController::clampWindowPosition(const QPoint& topLeft, const QSize& size)
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
