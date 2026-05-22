#pragma once

#include <QList>
#include <QMap>
#include <QObject>
#include <QPoint>
#include <QSize>

#include "domain/models/Config.h"

class QMainWindow;
class QSplitter;
class ServerTableView;
class ServerWorkspaceWidget;
class SharePanelWidget;

class WindowLayoutStateController final : public QObject {
public:
    WindowLayoutStateController(
        QMainWindow* window,
        ServerWorkspaceWidget* serverWorkspace,
        ServerTableView* serverView,
        SharePanelWidget* sharePanel);

    static QList<int> defaultServerColumnWidths();

    bool qrPreviewVisible() const;

    void restoreFromConfig(const Config& config);
    void captureToConfig(Config& config) const;
    void handleShowEvent();
    void handleQrPreviewToggled(bool checked);
    void handleTopSplitterMoved();
    void handleRootSplitterMoved();

private:
    void applyDeferredUiState();
    void applyFrameAdjustedWindowMetrics();
    void restoreServerColumnWidths(const QMap<QString, int>& widths);
    QMap<QString, int> captureServerColumnWidths() const;
    int captureSplitPercent(QSplitter* splitter, int fallback) const;
    void applySplitPercent(QSplitter* splitter, int percent);

    static int clampSplitPercent(int value, int fallback);
    static QStringList serverColumnKeys();
    static QPoint clampWindowPosition(const QPoint& topLeft, const QSize& size);

    QMainWindow* window_ = nullptr;
    ServerWorkspaceWidget* serverWorkspace_ = nullptr;
    ServerTableView* serverView_ = nullptr;
    SharePanelWidget* sharePanel_ = nullptr;
    bool qrPreviewVisible_ = false;
    bool uiStateRestorePending_ = false;
    bool initialServerColumnLayoutPending_ = true;
    bool frameAdjustedWindowMetricsApplied_ = false;
    bool applyingServerColumnWidths_ = false;
    bool serverColumnsManuallyAdjusted_ = false;
    QMap<QString, int> pendingColumnWidths_;
    int serverLogSplitPercent_ = 60;
    int serverQrSplitPercent_ = 78;
};
