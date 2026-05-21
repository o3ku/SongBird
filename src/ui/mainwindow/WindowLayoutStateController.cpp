#include "ui/mainwindow/WindowLayoutStateController.h"

#include <QGuiApplication>
#include <QHeaderView>
#include <QMainWindow>
#include <QPoint>
#include <QScreen>
#include <QSplitter>
#include <QTableView>
#include <QTimer>

#include "ui/mainwindow/ServerTableView.h"
#include "ui/mainwindow/ServerWorkspaceWidget.h"
#include "ui/mainwindow/SharePanelWidget.h"

namespace {

constexpr int DefaultServerLogSplitPercent = 60;
constexpr int DefaultServerQrSplitPercent = 78;
constexpr int DefaultMainWindowWidth = 1000;
constexpr int DefaultMainWindowHeight = 640;
constexpr int ManualServerColumnWidthsMarker = 1;
constexpr int ServerResultColumn = 4;

QStringList defaultServerColumnKeys()
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

    if (serverView_ != nullptr && serverView_->horizontalHeader() != nullptr) {
        connect(serverView_->horizontalHeader(), &QHeaderView::sectionResized, this, [this](int logicalIndex, int, int) {
            if (!applyingServerColumnWidths_ && logicalIndex != ServerResultColumn) {
                serverColumnsManuallyAdjusted_ = true;
            }
        });
    }
}

QList<int> WindowLayoutStateController::defaultServerColumnWidths(int availableWidth)
{
    Q_UNUSED(availableWidth);
    QList<int> widths{
        44,
        90,
        220,
        320,
        84
    };

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
        qMax(window_->minimumWidth(), config.ui().mainSizeWidth > 0 ? config.ui().mainSizeWidth : window_->width()),
        qMax(window_->minimumHeight(), config.ui().mainSizeHeight > 0 ? config.ui().mainSizeHeight : window_->height()));
    if (restoredSize.width() > 0 && restoredSize.height() > 0) {
        window_->resize(restoredSize);
    }

    if (config.ui().mainLocationX != 0 || config.ui().mainLocationY != 0) {
        window_->move(clampWindowPosition(QPoint(config.ui().mainLocationX, config.ui().mainLocationY), window_->size()));
    }

    pendingColumnWidths_ = hasManualServerColumnWidths(config.ui().mainColumnWidths)
        ? config.ui().mainColumnWidths
        : QMap<QString, int>();
    serverLogSplitPercent_ = clampSplitPercent(config.ui().mainServerLogSplitPercent, DefaultServerLogSplitPercent);
    serverQrSplitPercent_ = clampSplitPercent(config.ui().mainServerQrSplitPercent, DefaultServerQrSplitPercent);
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
    } else if (!hasManualServerColumnWidths(config.ui().mainColumnWidths)) {
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

    if (hasLegacyServerColumnWidths(widths) || !hasManualServerColumnWidths(widths)) {
        return;
    }

    QMap<QString, int> effectiveWidths = widths;
    effectiveWidths.remove(manualServerColumnWidthsKey());
    const QStringList keys = serverColumnKeys();
    QHeaderView* header = serverView_->horizontalHeader();
    applyingServerColumnWidths_ = true;
    for (int index = 0; index < keys.size() && index < header->count(); ++index) {
        const auto it = effectiveWidths.constFind(keys.at(index));
        if (it != effectiveWidths.constEnd() && it.value() > 0) {
            header->resizeSection(index, it.value());
        }
    }
    applyingServerColumnWidths_ = false;
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
        if (index == ServerResultColumn) {
            continue;
        }
        widths.insert(keys.at(index), header->sectionSize(index));
    }
    widths.insert(manualServerColumnWidthsKey(), ManualServerColumnWidthsMarker);

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
