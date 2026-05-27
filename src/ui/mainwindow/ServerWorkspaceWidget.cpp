#include "ui/mainwindow/ServerWorkspaceWidget.h"

#include <QAbstractItemView>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QLayoutItem>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QSizePolicy>
#include <QSplitter>
#include <QStyle>
#include <QStyleOptionToolButton>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>

#include "ui/mainwindow/LogPanelWidget.h"
#include "ui/mainwindow/ServerTableView.h"
#include "ui/mainwindow/SharePanelWidget.h"
#include "ui/models/ServerFilterProxyModel.h"
#include "ui/theme/AppTheme.h"

namespace {

constexpr int HeaderFilterMinimumCharacters = 14;
constexpr int ServerResultColumn = 4;
constexpr int CompactPanelPadding = 4;
constexpr int CompactSectionSpacing = 4;
constexpr int CompactHeaderHorizontalPadding = 10;
constexpr int CompactHeaderIconTextSpacing = 10;

void paintCompactDisclosureArrow(QPainter& painter, const QRect& rect, bool expanded, const QColor& color)
{
    if (!rect.isValid()) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

    const QPointF center(rect.center());
    QPainterPath arrow;
    if (expanded) {
        arrow.moveTo(center.x() - 4.0, center.y() - 2.0);
        arrow.lineTo(center.x(), center.y() + 2.5);
        arrow.lineTo(center.x() + 4.0, center.y() - 2.0);
    } else {
        arrow.moveTo(center.x() - 2.0, center.y() - 4.0);
        arrow.lineTo(center.x() + 2.5, center.y());
        arrow.lineTo(center.x() - 2.0, center.y() + 4.0);
    }

    painter.drawPath(arrow);
    painter.restore();
}

class CompactSubscriptionHeader final : public QToolButton {
public:
    CompactSubscriptionHeader(const QString& tabKey, QWidget* parent = nullptr)
        : QToolButton(parent)
        , tabKey_(tabKey)
    {
        setObjectName(QStringLiteral("compactSubscriptionHeader"));
        setProperty("tabKey", tabKey_);
        setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        setLayoutDirection(Qt::LeftToRight);
        setIconSize(QSize(12, 12));
        setAutoRaise(true);
        setCheckable(true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setContextMenuPolicy(Qt::CustomContextMenu);
        AppTheme::applyCompactFont(this);
    }

    QString tabKey() const
    {
        return tabKey_;
    }

    void setExpanded(bool expanded)
    {
        setChecked(expanded);
        setProperty("expanded", expanded);
        QFont titleFont = font();
        titleFont.setBold(expanded);
        setFont(titleFont);
        refreshThemeAssets();
        setText(label_);
        style()->unpolish(this);
        style()->polish(this);
        update();
    }

    void setLabel(const QString& label)
    {
        label_ = label;
        setExpanded(isChecked());
    }

    Q_INVOKABLE void refreshThemeAssets() { update(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QStyleOptionToolButton option;
        initStyleOption(&option);

        QStyleOptionToolButton panelOption = option;
        panelOption.text.clear();
        panelOption.icon = QIcon();

        QPainter painter(this);
        style()->drawComplexControl(QStyle::CC_ToolButton, &panelOption, &painter, this);

        const QSize currentIconSize = iconSize();
        const QRect contentRect = rect().adjusted(
            CompactHeaderHorizontalPadding,
            0,
            -CompactHeaderHorizontalPadding,
            0);
        const QRect iconRect(
            contentRect.left(),
            contentRect.top() + ((contentRect.height() - currentIconSize.height()) / 2),
            currentIconSize.width(),
            currentIconSize.height());
        const QColor textColor = option.palette.color(QPalette::ButtonText);
        paintCompactDisclosureArrow(painter, iconRect, isChecked(), textColor);

        const QRect textRect = contentRect.adjusted(
            currentIconSize.width() + CompactHeaderIconTextSpacing,
            0,
            0,
            0);
        painter.setFont(font());
        painter.setPen(textColor);
        painter.drawText(
            textRect,
            Qt::AlignLeft | Qt::AlignVCenter,
            fontMetrics().elidedText(text(), Qt::ElideRight, textRect.width()));
    }

private:
    QString tabKey_;
    QString label_;
};

int textControlMinimumWidth(const QWidget* widget, const QString& text, int minimumCharacters, int chromeWidth)
{
    if (widget == nullptr) {
        return 0;
    }

    const QFontMetrics metrics(widget->font());
    const int textWidth = metrics.horizontalAdvance(text);
    const int characterWidth = metrics.horizontalAdvance(QString(minimumCharacters, QLatin1Char('M')));
    return qMax(textWidth, characterWidth) + chromeWidth;
}

void configureContentSizedLineEdit(QLineEdit* edit, int minimumCharacters)
{
    if (edit == nullptr) {
        return;
    }

    edit->setMinimumWidth(textControlMinimumWidth(edit, edit->placeholderText(), minimumCharacters, 40));
    edit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void refreshStyle(QWidget* widget)
{
    if (widget == nullptr) {
        return;
    }

    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

} // namespace

ServerWorkspaceWidget::ServerWorkspaceWidget(
    ServerFilterProxyModel* serverFilterModel,
    bool qrPreviewVisible,
    const QList<int>& defaultColumnWidths,
    QWidget* parent)
    : QWidget(parent)
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    serverView_ = new ServerTableView(this);
    serverView_->setObjectName(QStringLiteral("serverTableView"));
    serverView_->setProperty("compactServerTable", false);
    serverView_->setModel(serverFilterModel);
    serverView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    serverView_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    serverView_->setSortingEnabled(false);
    serverView_->setMinimumHeight(0);
    serverView_->setFrameShape(QFrame::NoFrame);
    serverView_->setContentsMargins(0, 0, 0, 0);
    serverView_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    serverView_->verticalHeader()->setVisible(false);
    const int rowHeight = serverView_->fontMetrics().height() + 8;
    serverView_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    serverView_->verticalHeader()->setDefaultSectionSize(rowHeight);
    serverView_->verticalHeader()->setMinimumSectionSize(rowHeight);
    serverView_->horizontalHeader()->setStretchLastSection(true);
    serverView_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    serverView_->horizontalHeader()->setSectionResizeMode(ServerResultColumn, QHeaderView::Stretch);
    serverView_->horizontalHeader()->setSectionsClickable(true);
    serverView_->horizontalHeader()->setHighlightSections(false);
    const int headerHeight = serverView_->horizontalHeader()->fontMetrics().height() + 8;
    serverView_->horizontalHeader()->setDefaultSectionSize(headerHeight);
    serverView_->horizontalHeader()->setMinimumSectionSize(headerHeight);
    serverView_->horizontalHeader()->setSortIndicatorShown(true);
    serverView_->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    for (int index = 0; index < defaultColumnWidths.size(); ++index) {
        serverView_->horizontalHeader()->resizeSection(index, defaultColumnWidths.at(index));
    }

    subscriptionTabBar_ = new QTabBar(this);
    subscriptionTabBar_->setObjectName(QStringLiteral("subscriptionTabBar"));
    AppTheme::applyCompactFont(subscriptionTabBar_);
    QFont subscriptionTabFont = subscriptionTabBar_->font();
    subscriptionTabFont.setBold(true);
    subscriptionTabBar_->setFont(subscriptionTabFont);
    subscriptionTabBar_->setContextMenuPolicy(Qt::CustomContextMenu);
    subscriptionTabBar_->setDocumentMode(true);
    subscriptionTabBar_->setDrawBase(false);
    subscriptionTabBar_->setExpanding(false);
    subscriptionTabBar_->setAutoHide(false);
    subscriptionTabBar_->setUsesScrollButtons(true);

    auto* subscriptionTabBarContainer = new QWidget(this);
    subscriptionTabBarContainer->setObjectName(QStringLiteral("subscriptionTabBarContainer"));
    subscriptionTabBarContainer->setAttribute(Qt::WA_StyledBackground, true);
    auto* subscriptionTabBarLayout = new QHBoxLayout(subscriptionTabBarContainer);
    subscriptionTabBarLayout->setContentsMargins(4, 4, 4, 4);
    subscriptionTabBarLayout->setSpacing(6);
    subscriptionTabBarLayout->addWidget(subscriptionTabBar_, 1, Qt::AlignVCenter);

    serverFilterEdit_ = new QLineEdit(this);
    serverFilterEdit_->setObjectName(QStringLiteral("serverFilterEdit"));
    AppTheme::applyCompactFont(serverFilterEdit_);
    serverFilterEdit_->setClearButtonEnabled(true);
    serverFilterEdit_->setPlaceholderText(tr("Filter servers"));
    configureContentSizedLineEdit(serverFilterEdit_, HeaderFilterMinimumCharacters);
    subscriptionTabBarLayout->addWidget(serverFilterEdit_, 0, Qt::AlignVCenter);

    sharePanel_ = new SharePanelWidget(this);
    sharePanel_->setPreviewVisible(qrPreviewVisible);

    serverPanel_ = new QWidget(this);
    serverPanel_->setMinimumHeight(0);
    serverPanel_->setProperty("sectionBody", true);
    serverPanelLayout_ = new QVBoxLayout(serverPanel_);
    serverPanelLayout_->setContentsMargins(0, 0, 0, 0);
    serverPanelLayout_->setSpacing(0);

    serverHeaderRow_ = new QWidget(serverPanel_);
    serverHeaderRow_->setObjectName(QStringLiteral("serverHeaderRow"));
    serverHeaderRow_->setAttribute(Qt::WA_StyledBackground, true);
    serverHeaderRow_->setProperty("sectionHeader", true);
    auto* serverHeaderLayout = new QHBoxLayout(serverHeaderRow_);
    serverHeaderLayout->setContentsMargins(0, 0, 0, 0);
    serverHeaderLayout->setSpacing(0);
    serverHeaderLayout->addWidget(subscriptionTabBarContainer);
    subscriptionTabBarContainer_ = subscriptionTabBarContainer;
    subscriptionTabBarLayout_ = subscriptionTabBarLayout;
    serverPanelLayout_->addWidget(serverHeaderRow_);
    serverPanelLayout_->addWidget(serverView_, 1);

    compactPanel_ = new QWidget(this);
    compactPanel_->setObjectName(QStringLiteral("compactServerPanel"));
    compactPanel_->setMinimumHeight(0);
    compactPanel_->setProperty("sectionBody", true);
    compactPanelLayout_ = new QVBoxLayout(compactPanel_);
    compactPanelLayout_->setContentsMargins(0, 0, 0, 0);
    compactPanelLayout_->setSpacing(0);
    compactPanel_->hide();

    compactSectionsWidget_ = new QWidget(compactPanel_);
    compactSectionsWidget_->setObjectName(QStringLiteral("compactSubscriptionSections"));
    compactSectionsWidget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    compactSectionsLayout_ = new QVBoxLayout(compactSectionsWidget_);
    compactSectionsLayout_->setContentsMargins(0, 0, 0, 0);
    compactSectionsLayout_->setSpacing(0);
    compactPanelLayout_->addWidget(compactSectionsWidget_, 1);

    topSplitter_ = new QSplitter(Qt::Horizontal, this);
    topSplitter_->setObjectName(QStringLiteral("topSplitter"));
    topSplitter_->setChildrenCollapsible(false);
    topSplitter_->addWidget(serverPanel_);
    topSplitter_->addWidget(sharePanel_);
    topSplitter_->addWidget(compactPanel_);
    topSplitter_->setStretchFactor(0, 4);
    topSplitter_->setStretchFactor(1, 1);
    topSplitter_->setStretchFactor(2, 4);
    topSplitter_->setCollapsible(0, false);
    topSplitter_->setCollapsible(1, false);
    topSplitter_->setCollapsible(2, false);

    logPanel_ = new LogPanelWidget(this);
    logPanel_->setMinimumHeight(0);

    rootSplitter_ = new QSplitter(Qt::Vertical, this);
    rootSplitter_->setObjectName(QStringLiteral("rootSplitter"));
    rootSplitter_->setContentsMargins(0, 0, 0, 0);
    rootSplitter_->setChildrenCollapsible(false);
    rootSplitter_->addWidget(topSplitter_);
    rootSplitter_->addWidget(logPanel_);
    rootSplitter_->setStretchFactor(0, 4);
    rootSplitter_->setStretchFactor(1, 1);
    rootSplitter_->setCollapsible(0, false);
    rootSplitter_->setCollapsible(1, false);

    outerLayout->addWidget(rootSplitter_);
}

ServerTableView* ServerWorkspaceWidget::serverView() const
{
    return serverView_;
}

QTabBar* ServerWorkspaceWidget::subscriptionTabBar() const
{
    return subscriptionTabBar_;
}

QLineEdit* ServerWorkspaceWidget::serverFilterEdit() const
{
    return serverFilterEdit_;
}

SharePanelWidget* ServerWorkspaceWidget::sharePanel() const
{
    return sharePanel_;
}

LogPanelWidget* ServerWorkspaceWidget::logPanel() const
{
    return logPanel_;
}

QSplitter* ServerWorkspaceWidget::topSplitter() const
{
    return topSplitter_;
}

QSplitter* ServerWorkspaceWidget::rootSplitter() const
{
    return rootSplitter_;
}

void ServerWorkspaceWidget::setCompactMode(bool compact)
{
    if (compactMode_ == compact) {
        syncCompactSections();
        return;
    }

    compactMode_ = compact;
    if (compactMode_) {
        if (compactPanelLayout_ != nullptr) {
            compactPanelLayout_->setContentsMargins(
                CompactPanelPadding,
                CompactPanelPadding,
                CompactPanelPadding,
                CompactPanelPadding);
            compactPanelLayout_->setSpacing(CompactSectionSpacing);
        }
        if (serverPanel_ != nullptr) {
            serverPanel_->hide();
        }
        if (compactPanel_ != nullptr) {
            compactPanel_->show();
        }
        if (sharePanel_ != nullptr) {
            sharePanel_->hide();
        }
        moveServerFilterToCompact();
        rebuildCompactSections();
        moveServerViewToCompact();
    } else {
        if (compactPanelLayout_ != nullptr) {
            compactPanelLayout_->setContentsMargins(0, 0, 0, 0);
            compactPanelLayout_->setSpacing(0);
        }
        moveServerFilterToDesktop();
        moveServerViewToDesktop();
        if (compactPanel_ != nullptr) {
            compactPanel_->hide();
        }
        if (serverPanel_ != nullptr) {
            serverPanel_->show();
        }
        if (sharePanel_ != nullptr) {
            sharePanel_->setVisible(sharePanel_->previewVisible());
        }
    }
}

bool ServerWorkspaceWidget::compactMode() const
{
    return compactMode_;
}

void ServerWorkspaceWidget::syncCompactSections()
{
    if (compactMode_) {
        rebuildCompactSections();
        moveServerViewToCompact();
    }
}

void ServerWorkspaceWidget::setCompactContextMenuHandler(std::function<void(const QString&, const QPoint&)> handler)
{
    compactContextMenuHandler_ = std::move(handler);
}

void ServerWorkspaceWidget::setCompactSelectionHandler(std::function<void(const QString&)> handler)
{
    compactSelectionHandler_ = std::move(handler);
}

void ServerWorkspaceWidget::rebuildCompactSections()
{
    if (compactSectionsLayout_ == nullptr || subscriptionTabBar_ == nullptr) {
        return;
    }

    const QString currentTabKey = subscriptionTabBar_->currentIndex() < 0
        ? QStringLiteral("ungrouped")
        : subscriptionTabBar_->tabData(subscriptionTabBar_->currentIndex()).toString();

    while (compactSectionsLayout_->count() > 0) {
        QLayoutItem* item = compactSectionsLayout_->takeAt(0);
        if (item->widget() != nullptr && item->widget() != serverView_) {
            item->widget()->hide();
            item->widget()->deleteLater();
        }
        delete item;
    }

    for (int index = 0; index < subscriptionTabBar_->count(); ++index) {
        const QString tabKey = subscriptionTabBar_->tabData(index).toString();
        auto* header = new CompactSubscriptionHeader(tabKey, compactSectionsWidget_);
        header->setLabel(subscriptionTabBar_->tabText(index));
        header->setExpanded(tabKey == currentTabKey);
        const auto selectSection = [this, tabKey]() {
            if (subscriptionTabBar_ == nullptr) {
                return;
            }
            for (int tabIndex = 0; tabIndex < subscriptionTabBar_->count(); ++tabIndex) {
                if (subscriptionTabBar_->tabData(tabIndex).toString() == tabKey) {
                    subscriptionTabBar_->setCurrentIndex(tabIndex);
                    break;
                }
            }
            if (compactSelectionHandler_) {
                compactSelectionHandler_(tabKey);
            } else {
                syncCompactSections();
            }
        };
        connect(header, &QToolButton::clicked, this, [selectSection](bool) {
            selectSection();
        });
        connect(header, &QWidget::customContextMenuRequested, this, [this, header, tabKey](const QPoint& position) {
            if (compactContextMenuHandler_) {
                compactContextMenuHandler_(tabKey, header->mapToGlobal(position));
            }
        });
        compactSectionsLayout_->addWidget(header);

        if (tabKey == currentTabKey && serverView_ != nullptr) {
            compactSectionsLayout_->addWidget(serverView_, 1);
        }
        if (index + 1 < subscriptionTabBar_->count()) {
            compactSectionsLayout_->addSpacing(CompactSectionSpacing);
        }
    }
}

void ServerWorkspaceWidget::moveServerFilterToDesktop()
{
    if (subscriptionTabBarLayout_ == nullptr || serverFilterEdit_ == nullptr) {
        return;
    }

    serverFilterEdit_->setParent(subscriptionTabBarContainer_);
    subscriptionTabBarLayout_->addWidget(serverFilterEdit_, 0, Qt::AlignVCenter);
    serverFilterEdit_->show();
}

void ServerWorkspaceWidget::moveServerFilterToCompact()
{
    if (compactPanelLayout_ == nullptr || serverFilterEdit_ == nullptr) {
        return;
    }

    serverFilterEdit_->setParent(compactPanel_);
    compactPanelLayout_->insertWidget(0, serverFilterEdit_);
    serverFilterEdit_->show();
}

void ServerWorkspaceWidget::moveServerViewToDesktop()
{
    if (serverPanelLayout_ == nullptr || serverView_ == nullptr) {
        return;
    }

    serverView_->setParent(serverPanel_);
    serverView_->setProperty("compactServerTable", false);
    refreshStyle(serverView_);
    serverPanelLayout_->addWidget(serverView_, 1);
    serverView_->show();
}

void ServerWorkspaceWidget::moveServerViewToCompact()
{
    if (compactSectionsLayout_ == nullptr || serverView_ == nullptr) {
        return;
    }

    serverView_->setParent(compactSectionsWidget_);
    serverView_->setProperty("compactServerTable", true);
    refreshStyle(serverView_);
    serverView_->show();
}
