#pragma once

#include <QStringList>
#include <QWidget>

class QLabel;
class QPlainTextEdit;

class SharePanelWidget final : public QWidget {
    Q_OBJECT

public:
    explicit SharePanelWidget(QWidget* parent = nullptr);

    void setShareLinks(const QStringList& shareLinks);
    void setPreviewVisible(bool visible);

signals:
    void transientStatusRequested(const QString& message, int timeoutMs);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void refreshPreview();

    QWidget* shareContentPanel_ = nullptr;
    QLabel* qrPlaceholder_ = nullptr;
    QPlainTextEdit* shareLinkLabel_ = nullptr;
    QStringList shareLinks_;
    bool previewVisible_ = false;
};
