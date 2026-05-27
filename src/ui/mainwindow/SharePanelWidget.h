#pragma once

#include <QPixmap>
#include <QString>
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
    bool previewVisible() const;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void refreshPreview();
    void clearPreviewPixmap();

    QWidget* shareContentPanel_ = nullptr;
    QLabel* qrPlaceholder_ = nullptr;
    QPlainTextEdit* shareLinkLabel_ = nullptr;
    QString shareText_;
    QString firstShareUrl_;
    QPixmap cachedQrPixmap_;
    QString cachedQrShareUrl_;
    int cachedQrExtent_ = 0;
    bool previewVisible_ = false;
};
