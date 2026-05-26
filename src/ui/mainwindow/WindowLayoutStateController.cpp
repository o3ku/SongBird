#include "ui/mainwindow/WindowLayoutStateController.h"

#include <QHeaderView>
#include <QMainWindow>
#include <QSplitter>
#include <QTimer>

#include "ui/mainwindow/ServerTableView.h"
#include "ui/mainwindow/ServerWorkspaceWidget.h"
#include "ui/mainwindow/SharePanelWidget.h"
#include "ui/mainwindow/WindowLayoutStateSupport.h"

namespace Layout = WindowLayoutStateSupport;

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

    if (serverView_ != nullptr && serverView_->horizontalHeader() != nullptr) {
        connect(serverView_->horizontalHeader(), &QHeaderView::sectionResized, this, [this](int logicalIndex, int, int) {
            if (!applyingServerColumnWidths_ && logicalIndex != Layout::ServerResultColumn) {
                serverColumnsManuallyAdjusted_ = true;
            }
        });
    }
}

QList<int> WindowLayoutStateController::defaultServerColumnWidths()
{
    return Layout::defaultServerColumnWidths();
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
        qMax(window_->minimumWidth(), config.ui().mainSizeWidth > 0 ? config.ui().mainSizeWidth : window_->width()),
        qMax(window_->minimumHeight(), config.ui().mainSizeHeight > 0 ? config.ui().mainSizeHeight : window_->height()));
    if (restoredSize.width() > 0 && restoredSize.height() > 0) {
        window_->resize(restoredSize);
    }

    if (config.ui().mainLocationX != 0 || config.ui().mainLocationY != 0) {
        window_->move(Layout::clampWindowPosition(QPoint(config.ui().mainLocationX, config.ui().mainLocationY), window_->size()));
    }

    pendingColumnWidths_ = Layout::hasManualServerColumnWidths(config.ui().mainColumnWidths)
        ? config.ui().mainColumnWidths
        : QMap<QString, int>();
    serverLogSplitPercent_ = Layout::clampSplitPercent(config.ui().mainServerLogSplitPercent, Layout::DefaultServerLogSplitPercent);
    serverQrSplitPercent_ = Layout::clampSplitPercent(config.ui().mainServerQrSplitPercent, Layout::DefaultServerQrSplitPercent);
    qrPreviewVisible_ = config.ui().mainQrPreviewVisible;
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

    config.ui().mainLocationX = bounds.x();
    config.ui().mainLocationY = bounds.y();
    config.ui().mainSizeWidth = bounds.width();
    config.ui().mainSizeHeight = bounds.height();
    if (serverColumnsManuallyAdjusted_) {
        config.ui().mainColumnWidths = captureServerColumnWidths();
    } else if (!Layout::hasManualServerColumnWidths(config.ui().mainColumnWidths)) {
        config.ui().mainColumnWidths.clear();
    }
    config.ui().mainServerLogSplitPercent = captureSplitPercent(
        serverWorkspace_ == nullptr ? nullptr : serverWorkspace_->rootSplitter(),
        serverLogSplitPercent_);
    config.ui().mainServerQrSplitPercent = captureSplitPercent(
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
        qMax(1, Layout::DefaultMainWindowWidth - frameDelta.width()),
        qMax(1, Layout::DefaultMainWindowHeight - frameDelta.height()));

    frameAdjustedWindowMetricsApplied_ = true;

    if (clientSize == QSize(Layout::DefaultMainWindowWidth, Layout::DefaultMainWindowHeight)) {
        window_->resize(targetClientSize);
    }
}

void WindowLayoutStateController::restoreServerColumnWidths(const QMap<QString, int>& widths)
{
    if (serverView_ == nullptr || serverView_->horizontalHeader() == nullptr) {
        return;
    }

    QHeaderView* header = serverView_->horizontalHeader();
    applyingServerColumnWidths_ = true;
    Layout::restoreServerColumnWidths(header, widths);
    applyingServerColumnWidths_ = false;
}

QMap<QString, int> WindowLayoutStateController::captureServerColumnWidths() const
{
    if (serverView_ == nullptr || serverView_->horizontalHeader() == nullptr) {
        return {};
    }

    return Layout::captureServerColumnWidths(serverView_->horizontalHeader());
}

int WindowLayoutStateController::captureSplitPercent(QSplitter* splitter, int fallback) const
{
    if (splitter == nullptr) {
        return Layout::clampSplitPercent(fallback, fallback);
    }

    if (serverWorkspace_ != nullptr
        && splitter == serverWorkspace_->topSplitter()
        && (sharePanel_ == nullptr || !sharePanel_->isVisible())) {
        return Layout::clampSplitPercent(serverQrSplitPercent_, fallback);
    }

    return Layout::captureSplitPercent(splitter, fallback);
}

void WindowLayoutStateController::applySplitPercent(QSplitter* splitter, int percent)
{
    Layout::applySplitPercent(
        splitter,
        percent,
        serverWorkspace_ != nullptr && splitter == serverWorkspace_->rootSplitter()
            ? Layout::DefaultServerLogSplitPercent
            : Layout::DefaultServerQrSplitPercent);
}
