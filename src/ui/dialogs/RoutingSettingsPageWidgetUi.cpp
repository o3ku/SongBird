#include "ui/dialogs/RoutingSettingsPageWidget.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTabBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>

#include "ui/dialogs/RoutingCustomRuleSupport.h"
#include "ui/theme/AppTheme.h"

namespace {

void applyBoldSectionTitle(QLabel* label)
{
    AppTheme::applyCompactFont(label);
    QFont titleFont = label->font();
    titleFont.setBold(true);
    label->setFont(titleFont);
}

} // namespace

void RoutingSettingsPageWidget::setupUi()
{
    auto* routingLayout = new QVBoxLayout(this);

    baseRouteTitleLabel_ = new QLabel(tr("Base Route"), this);
    baseRouteTitleLabel_->setObjectName(QStringLiteral("routingBaseRouteTitle"));
    applyBoldSectionTitle(baseRouteTitleLabel_);
    routingLayout->addWidget(baseRouteTitleLabel_);

    baseRouteLayout_ = new QHBoxLayout();
    baseRouteLayout_->setObjectName(QStringLiteral("routingBaseRouteLayout"));
    baseRouteLayout_->setContentsMargins(0, 0, 0, 0);
    baseRouteLayout_->setSpacing(8);
    baseRouteButtonGroup_ = new QButtonGroup(this);
    baseRouteButtonGroup_->setExclusive(true);
    connect(baseRouteButtonGroup_, &QButtonGroup::idClicked, this, [this](int) {
        updateBaseRouteCardGeometry();
    });
    routingLayout->addLayout(baseRouteLayout_);

    customRulesTitleLabel_ = new QLabel(tr("Custom Rules"), this);
    customRulesTitleLabel_->setObjectName(QStringLiteral("routingCustomRulesTitle"));
    applyBoldSectionTitle(customRulesTitleLabel_);
    routingLayout->addWidget(customRulesTitleLabel_);

    customRuleTabs_ = new QTabWidget(this);
    customRuleTabs_->setObjectName(QStringLiteral("routingCustomRuleTabs"));
    AppTheme::applyCompactFont(customRuleTabs_);
    customRuleTabs_->setDocumentMode(true);
    customRuleTabs_->tabBar()->setDrawBase(false);
    customRuleTabs_->tabBar()->setExpanding(false);
    AppTheme::applyCompactFont(customRuleTabs_->tabBar());

    for (const auto& tab : RoutingCustomRuleSupport::customRuleTabs()) {
        const QString action = tab.first;
        const QString title = tab.second;
        auto* tabWidget = new QWidget(customRuleTabs_);
        tabWidget->setProperty("routingCustomRulePage", true);
        auto* tabLayout = new QHBoxLayout(tabWidget);
        tabLayout->setContentsMargins(9, 9, 9, 9);
        tabLayout->setSpacing(12);

        CustomRuleEditors editors;

        auto* protocolPortColumn = new QWidget(tabWidget);
        auto* protocolPortLayout = new QVBoxLayout(protocolPortColumn);
        protocolPortLayout->setContentsMargins(0, 0, 0, 0);
        protocolPortLayout->setSpacing(4);
        auto* protocolLabel = new QLabel(tr("Protocol"), protocolPortColumn);
        editors.protocolEdit = new QTextEdit(protocolPortColumn);
        editors.protocolEdit->setObjectName(QStringLiteral("routingCustom%1ProtocolEdit").arg(title));
        editors.protocolEdit->setTabChangesFocus(true);
        editors.protocolEdit->setPlaceholderText(QStringLiteral("tcp\nudp"));
        auto* portLabel = new QLabel(tr("Port"), protocolPortColumn);
        editors.portEdit = new QLineEdit(protocolPortColumn);
        editors.portEdit->setObjectName(QStringLiteral("routingCustom%1PortEdit").arg(title));
        editors.portEdit->setPlaceholderText(QStringLiteral("0-65535"));
        AppTheme::applyCompactFont({protocolLabel, editors.protocolEdit, portLabel, editors.portEdit});
        protocolPortLayout->addWidget(protocolLabel);
        protocolPortLayout->addWidget(editors.protocolEdit, 1);
        protocolPortLayout->addWidget(portLabel);
        protocolPortLayout->addWidget(editors.portEdit);

        auto* ipColumn = new QWidget(tabWidget);
        auto* ipLayout = new QVBoxLayout(ipColumn);
        ipLayout->setContentsMargins(0, 0, 0, 0);
        ipLayout->setSpacing(4);
        auto* ipLabel = new QLabel(QStringLiteral("IP"), ipColumn);
        editors.ipEdit = new QTextEdit(ipColumn);
        editors.ipEdit->setObjectName(QStringLiteral("routingCustom%1IpEdit").arg(title));
        editors.ipEdit->setTabChangesFocus(true);
        editors.ipEdit->setPlaceholderText(QStringLiteral("geoip:private\n10.0.0.0/8"));
        AppTheme::applyCompactFont({ipLabel, editors.ipEdit});
        ipLayout->addWidget(ipLabel);
        ipLayout->addWidget(editors.ipEdit, 1);

        auto* domainColumn = new QWidget(tabWidget);
        auto* domainLayout = new QVBoxLayout(domainColumn);
        domainLayout->setContentsMargins(0, 0, 0, 0);
        domainLayout->setSpacing(4);
        auto* domainLabel = new QLabel(tr("Domain"), domainColumn);
        editors.domainEdit = new QTextEdit(domainColumn);
        editors.domainEdit->setObjectName(QStringLiteral("routingCustom%1DomainEdit").arg(title));
        editors.domainEdit->setTabChangesFocus(true);
        editors.domainEdit->setPlaceholderText(QStringLiteral("geosite:cn\ndomain:example.com"));
        AppTheme::applyCompactFont({domainLabel, editors.domainEdit});
        domainLayout->addWidget(domainLabel);
        domainLayout->addWidget(editors.domainEdit, 1);

        tabLayout->addWidget(protocolPortColumn, 1);
        tabLayout->addWidget(ipColumn, 1);
        tabLayout->addWidget(domainColumn, 1);

        tabWidget->setProperty("routingCustomRuleTabKey", action);
        customRuleTabs_->addTab(tabWidget, title);
        customRuleEditors_.insert(action, editors);
    }
    routingLayout->addWidget(customRuleTabs_, 1);

    connect(customRuleTabs_, &QTabWidget::currentChanged, this, [this]() {
        updateRoutingActionState();
    });
}
