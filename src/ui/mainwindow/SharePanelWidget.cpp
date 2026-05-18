#include "ui/mainwindow/SharePanelWidget.h"

#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QFrame>
#include <QLabel>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSizePolicy>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextOption>
#include <QVBoxLayout>
#include <QtMath>

#include "ui/qr/QrCodeRenderer.h"
#include "ui/theme/AppTheme.h"

namespace {

constexpr int QrPreviewPadding = 10;
constexpr int ShareLinkHorizontalPadding = 6;
constexpr int ShareLinkBottomPadding = 10;

class ShareLinkTextEdit final : public QPlainTextEdit {
public:
    explicit ShareLinkTextEdit(QWidget* parent = nullptr)
        : QPlainTextEdit(parent)
    {
        setReadOnly(true);
        setFrameStyle(QFrame::NoFrame);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setLineWrapMode(QPlainTextEdit::WidgetWidth);
        setWordWrapMode(QTextOption::WrapAnywhere);
        setBackgroundVisible(false);
        setViewportMargins(ShareLinkHorizontalPadding, 0, ShareLinkHorizontalPadding, ShareLinkBottomPadding);
        setProperty("shareLinkBottomPadding", ShareLinkBottomPadding);
        document()->setDocumentMargin(0.0);

        connect(document(), &QTextDocument::contentsChanged, this, [this]() {
            updateGeometry();
        });
    }

    bool hasHeightForWidth() const override
    {
        return true;
    }

    int heightForWidth(int width) const override
    {
        return documentHeightForWidth(width);
    }

    QSize sizeHint() const override
    {
        const int widthHint = qMax(minimumWidth(), width() > 0 ? width() : minimumWidth());
        return QSize(QPlainTextEdit::sizeHint().width(), documentHeightForWidth(widthHint));
    }

    QSize minimumSizeHint() const override
    {
        const int minimumHeight = fontMetrics().lineSpacing() + ShareLinkBottomPadding;
        return QSize(QPlainTextEdit::minimumSizeHint().width(), minimumHeight);
    }

private:
    int documentHeightForWidth(int width) const
    {
        if (width <= 0) {
            return 0;
        }

        QTextDocument document;
        document.setDefaultFont(font());
        QTextOption option = document.defaultTextOption();
        option.setWrapMode(QTextOption::WrapAnywhere);
        document.setDefaultTextOption(option);
        document.setDocumentMargin(0.0);
        document.setPlainText(toPlainText());
        document.setTextWidth(qMax(0, width - (ShareLinkHorizontalPadding * 2)));
        return qCeil(document.size().height()) + ShareLinkBottomPadding;
    }
};

} // namespace

SharePanelWidget::SharePanelWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("qrPanel"));
    setMinimumWidth(260);
    setMinimumHeight(0);

    auto* qrLayout = new QVBoxLayout(this);
    qrLayout->setContentsMargins(0, 0, 0, 0);
    qrLayout->setSpacing(0);

    shareContentPanel_ = new QWidget(this);
    shareContentPanel_->setObjectName(QStringLiteral("shareContentPanel"));
    shareContentPanel_->setMinimumWidth(260);
    shareContentPanel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    auto* shareContentLayout = new QVBoxLayout(shareContentPanel_);
    shareContentLayout->setContentsMargins(0, 0, 0, 0);
    shareContentLayout->setSpacing(0);

    qrPlaceholder_ = new QLabel(tr("QR preview placeholder"), shareContentPanel_);
    qrPlaceholder_->setObjectName(QStringLiteral("qrPlaceholder"));
    qrPlaceholder_->setAlignment(Qt::AlignCenter);
    qrPlaceholder_->setMargin(QrPreviewPadding);
    qrPlaceholder_->setMinimumHeight(0);
    qrPlaceholder_->setMinimumWidth(260);
    qrPlaceholder_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    qrPlaceholder_->installEventFilter(this);
    shareContentLayout->addWidget(qrPlaceholder_, 1);

    shareLinkLabel_ = new ShareLinkTextEdit(shareContentPanel_);
    shareLinkLabel_->setObjectName(QStringLiteral("shareLinkLabel"));
    shareLinkLabel_->setMinimumWidth(260);
    shareLinkLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    shareLinkLabel_->installEventFilter(this);
    AppTheme::applyCompactFont(shareLinkLabel_);
    shareContentLayout->addWidget(shareLinkLabel_, 0);

    qrLayout->addWidget(shareContentPanel_, 1);
    setVisible(previewVisible_);
}

void SharePanelWidget::setShareLinks(const QStringList& shareLinks)
{
    shareLinks_.clear();
    for (const QString& shareLink : shareLinks) {
        const QString trimmedShareLink = shareLink.trimmed();
        if (!trimmedShareLink.isEmpty()) {
            shareLinks_.append(trimmedShareLink);
        }
    }

    refreshPreview();
}

void SharePanelWidget::setPreviewVisible(bool visible)
{
    previewVisible_ = visible;
    setVisible(visible);
    if (previewVisible_) {
        refreshPreview();
    }
}

bool SharePanelWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == qrPlaceholder_ && event != nullptr && event->type() == QEvent::Resize) {
        refreshPreview();
    }

    if (watched == shareLinkLabel_ && event != nullptr && event->type() == QEvent::MouseButtonPress) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton && QApplication::clipboard() != nullptr) {
            const QTextCursor cursor = shareLinkLabel_->cursorForPosition(mouseEvent->pos());
            const QTextBlock block = cursor.block();
            const QString shareUrl = block.text().trimmed();
            if (!shareUrl.isEmpty()) {
                QApplication::clipboard()->setText(shareUrl);
                emit transientStatusRequested(tr("Copied share URL to the clipboard."), 2000);
                return true;
            }
        }
    }

    return QWidget::eventFilter(watched, event);
}

void SharePanelWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    refreshPreview();
}

void SharePanelWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    refreshPreview();
}

void SharePanelWidget::refreshPreview()
{
    if (qrPlaceholder_ == nullptr || shareLinkLabel_ == nullptr) {
        return;
    }

    if (shareLinks_.isEmpty()) {
        shareLinkLabel_->clear();
        shareLinkLabel_->setToolTip(QString());
        qrPlaceholder_->setPixmap(QPixmap());
        qrPlaceholder_->setText(tr("QR preview placeholder"));
        qrPlaceholder_->setToolTip(QString());
        return;
    }

    const QString firstShareUrl = shareLinks_.constFirst();
    if (shareLinks_.size() == 1) {
        shareLinkLabel_->setPlainText(firstShareUrl);
        shareLinkLabel_->setToolTip(firstShareUrl);
    } else {
        const QString shareText = shareLinks_.join(QChar('\n'));
        shareLinkLabel_->setPlainText(shareText);
        shareLinkLabel_->setToolTip(shareText);
    }

    if (!previewVisible_ || !isVisible()) {
        return;
    }

    if (firstShareUrl.isEmpty()) {
        qrPlaceholder_->setPixmap(QPixmap());
        qrPlaceholder_->setText(tr("QR preview unavailable for this server."));
        qrPlaceholder_->setToolTip(QString());
        return;
    }

    const int qrMargin = qrPlaceholder_->margin() * 2;
    const int qrExtent = qMin(
        qMax(0, qrPlaceholder_->width() - qrMargin),
        qMax(0, qrPlaceholder_->height() - qrMargin));
    qrPlaceholder_->setText(QString());
    if (qrExtent <= 0) {
        qrPlaceholder_->setPixmap(QPixmap());
        qrPlaceholder_->setToolTip(firstShareUrl);
        return;
    }

    qrPlaceholder_->setPixmap(QrCodeRenderer::render(firstShareUrl, qrExtent));
    qrPlaceholder_->setToolTip(firstShareUrl);
}
