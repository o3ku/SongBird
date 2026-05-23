#include "ui/mainwindow/ServerWorkspaceWidget.h"

#include <QAbstractItemView>
#include <QFontMetrics>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSizePolicy>
#include <QSplitter>
#include <QTabBar>
#include <QVBoxLayout>

#include "ui/mainwindow/LogPanelWidget.h"
#include "ui/mainwindow/ServerTableView.h"
#include "ui/mainwindow/SharePanelWidget.h"
#include "ui/models/ServerFilterProxyModel.h"
#include "ui/theme/AppTheme.h"

namespace {

constexpr int HeaderFilterMinimumCharacters = 14;
constexpr int ServerResultColumn = 4;

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
    subscriptionTabBarLayout->setContentsMargins(6, 4, 4, 4);
    subscriptionTabBarLayout->setSpacing(6);
    subscriptionTabBarLayout->addWidget(subscriptionTabBar_, 1, Qt::AlignVCenter);

    serverFilterEdit_ = new QLineEdit(this);
    serverFilterEdit_->setObjectName(QStringLiteral("serverFilterEdit"));
    AppTheme::applyCompactFont(serverFilterEdit_);
    serverFilterEdit_->setClearButtonEnabled(false);
    serverFilterEdit_->setPlaceholderText(tr("Filter servers"));
    configureContentSizedLineEdit(serverFilterEdit_, HeaderFilterMinimumCharacters);
    subscriptionTabBarLayout->addWidget(serverFilterEdit_, 0, Qt::AlignVCenter);

    sharePanel_ = new SharePanelWidget(this);
    sharePanel_->setPreviewVisible(qrPreviewVisible);

    auto* serverPanel = new QWidget(this);
    serverPanel->setMinimumHeight(0);
    serverPanel->setProperty("sectionBody", true);
    auto* serverPanelLayout = new QVBoxLayout(serverPanel);
    serverPanelLayout->setContentsMargins(0, 0, 0, 0);
    serverPanelLayout->setSpacing(0);

    auto* serverHeaderRow = new QWidget(serverPanel);
    serverHeaderRow->setObjectName(QStringLiteral("serverHeaderRow"));
    serverHeaderRow->setAttribute(Qt::WA_StyledBackground, true);
    serverHeaderRow->setProperty("sectionHeader", true);
    auto* serverHeaderLayout = new QHBoxLayout(serverHeaderRow);
    serverHeaderLayout->setContentsMargins(0, 0, 0, 0);
    serverHeaderLayout->setSpacing(0);
    serverHeaderLayout->addWidget(subscriptionTabBarContainer);
    serverPanelLayout->addWidget(serverHeaderRow);
    serverPanelLayout->addWidget(serverView_, 1);

    topSplitter_ = new QSplitter(Qt::Horizontal, this);
    topSplitter_->setObjectName(QStringLiteral("topSplitter"));
    topSplitter_->setChildrenCollapsible(false);
    topSplitter_->addWidget(serverPanel);
    topSplitter_->addWidget(sharePanel_);
    topSplitter_->setStretchFactor(0, 4);
    topSplitter_->setStretchFactor(1, 1);
    topSplitter_->setCollapsible(0, false);
    topSplitter_->setCollapsible(1, false);

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
