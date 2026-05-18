#include "ui/dialogs/RoutingSettingsPageWidget.h"

#include <algorithm>

#include <QButtonGroup>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QSizePolicy>
#include <QStyle>
#include <QStyleOptionButton>
#include <QTabBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>

#include "ui/theme/AppTheme.h"

namespace {

QString elideText(const QString& value, const QFontMetrics& metrics, int width)
{
    const QString trimmed = value.trimmed();
    if (metrics.horizontalAdvance(trimmed) <= width) {
        return trimmed;
    }

    const QString ellipsis = QStringLiteral("...");
    int length = trimmed.size();
    while (length > 0) {
        const QString candidate = trimmed.left(length).trimmed() + ellipsis;
        if (metrics.horizontalAdvance(candidate) <= width) {
            return candidate;
        }
        --length;
    }

    return ellipsis;
}

class BaseRouteCardButton final : public QPushButton
{
public:
    explicit BaseRouteCardButton(QWidget* parent = nullptr)
        : QPushButton(parent)
    {
        setProperty("routeTitleBold", true);
    }

    QSize sizeHint() const override
    {
        return cardSizeHint();
    }

    QSize minimumSizeHint() const override
    {
        return cardSizeHint();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);

        QStyleOptionButton option;
        initStyleOption(&option);
        option.text.clear();
        style()->drawControl(QStyle::CE_PushButtonBevel, &option, &painter, this);

        const QRect textRect = rect().adjusted(8, 8, -8, -8);
        const QStringList lines = text().split(QChar('\n'));
        QFont normalFont = font();
        QFont boldFont = normalFont;
        boldFont.setBold(true);

        painter.setPen(palette().buttonText().color());
        int y = textRect.top();
        for (int index = 0; index < lines.size(); ++index) {
            painter.setFont(index == 0 ? boldFont : normalFont);
            const QFontMetrics metrics(painter.font());
            const int lineHeight = metrics.lineSpacing();
            if (y + lineHeight > textRect.bottom() + 1) {
                break;
            }
            painter.drawText(
                QRect(textRect.left(), y, textRect.width(), lineHeight),
                Qt::AlignLeft | Qt::AlignVCenter,
                elideText(lines.at(index), metrics, textRect.width()));
            y += lineHeight;
        }
    }

private:
    QSize cardSizeHint() const
    {
        QSize hint = QPushButton::sizeHint();
        const QStringList lines = text().split(QChar('\n'));
        QFont normalFont = font();
        QFont boldFont = normalFont;
        boldFont.setBold(true);

        int textHeight = 0;
        for (int index = 0; index < lines.size(); ++index) {
            const QFontMetrics metrics(index == 0 ? boldFont : normalFont);
            textHeight += metrics.lineSpacing();
        }

        hint.setHeight(qMax(hint.height(), textHeight + 16));
        return hint;
    }
};

QString normalizedRoutingCustomRuleTabKey(QString key)
{
    key = key.trimmed().toLower();
    return key == QStringLiteral("block") || key == QStringLiteral("proxy")
        ? key
        : QStringLiteral("direct");
}

QString compactRuleValues(const QStringList& values)
{
    QStringList normalized;
    for (const QString& value : values) {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty()) {
            normalized.append(trimmed);
        }
    }
    return normalized.join(QStringLiteral(", "));
}

constexpr int kBaseRouteCardMaxWidth = 220;
constexpr int kBaseRouteCardTextWidth = 210;
constexpr int kBaseRouteCardCollapsedWidth = 100;

QString elideCardLine(const QString& value, const QFontMetrics& metrics)
{
    return elideText(value, metrics, kBaseRouteCardTextWidth);
}

QStringList routingRuleMatches(const RoutingRule& rule)
{
    QStringList matches;
    const QString port = rule.port.trimmed();
    if (!port.isEmpty()) {
        matches.append(QStringLiteral("port: %1").arg(port));
    }

    const QString network = rule.network.trimmed();
    if (!network.isEmpty()) {
        matches.append(QStringLiteral("network: %1").arg(network));
    }

    const QString protocols = compactRuleValues(rule.protocol);
    if (!protocols.isEmpty()) {
        matches.append(QStringLiteral("protocol: %1").arg(protocols));
    }

    const QString processes = compactRuleValues(rule.process);
    if (!processes.isEmpty()) {
        matches.append(QStringLiteral("process: %1").arg(processes));
    }

    const QString ips = compactRuleValues(rule.ip);
    if (!ips.isEmpty()) {
        matches.append(ips);
    }

    const QString domains = compactRuleValues(rule.domain);
    if (!domains.isEmpty()) {
        matches.append(domains);
    }

    matches.removeAll(QString());
    return matches;
}

QString routingRuleActionLabel(const RoutingRule& rule)
{
    const QString outbound = rule.outboundTag.trimmed().toLower();
    if (outbound == QStringLiteral("block")) {
        return QStringLiteral("BLOCK");
    }
    if (outbound == QStringLiteral("direct")) {
        return QStringLiteral("DIRECT");
    }
    if (outbound == QStringLiteral("proxy")) {
        return QStringLiteral("PROXY");
    }
    if (!outbound.isEmpty()) {
        return outbound.toUpper();
    }
    return QStringLiteral("RULE");
}

void appendCardRuleLine(QStringList& lines, const RoutingRule& rule, const QFontMetrics* metrics = nullptr)
{
    const QStringList matches = routingRuleMatches(rule);
    if (matches.isEmpty()) {
        return;
    }

    const QString line = QStringLiteral("%1: %2").arg(routingRuleActionLabel(rule), matches.join(QStringLiteral(", ")));
    lines.append(metrics == nullptr ? line : elideCardLine(line, *metrics));
}

QString baseRouteCardText(const RoutingItem& item, int index, const QFontMetrics& metrics)
{
    QStringList lines;
    const QString title = item.remarks.trimmed().isEmpty()
        ? QStringLiteral("Route %1").arg(index + 1)
        : item.remarks.trimmed();
    lines.append(elideCardLine(title, metrics));
    for (const RoutingRule& rule : item.rules) {
        if (!rule.enabled) {
            continue;
        }
        appendCardRuleLine(lines, rule, &metrics);
    }
    return lines.join(QChar('\n'));
}

QString baseRouteCardFullText(const RoutingItem& item, int index)
{
    QStringList lines;
    lines.append(item.remarks.trimmed().isEmpty()
        ? QStringLiteral("Route %1").arg(index + 1)
        : item.remarks.trimmed());
    for (const RoutingRule& rule : item.rules) {
        if (!rule.enabled) {
            continue;
        }
        appendCardRuleLine(lines, rule);
    }
    return lines.join(QChar('\n'));
}

void clearLayout(QLayout* layout)
{
    if (layout == nullptr) {
        return;
    }

    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
}

} // namespace

RoutingSettingsPageWidget::RoutingSettingsPageWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void RoutingSettingsPageWidget::setConfig(const Config& config)
{
    config_ = config;
    routingItems_ = config.routingItems;
    reloadRoutingPresentation(findInitialRouteIndex(routingItems_, config));
    loadRoutingCustomRules(config.routingCustomRules);
    selectRoutingCustomRuleTab(config.settingsRoutingRuleTabKey);
    updateRoutingActionState();
}

QList<RoutingItem> RoutingSettingsPageWidget::routingItems() const
{
    return collectRoutingItems();
}

QList<RoutingRule> RoutingSettingsPageWidget::routingCustomRules() const
{
    return collectRoutingCustomRules();
}

QString RoutingSettingsPageWidget::settingsRoutingRuleTabKey() const
{
    return selectedRoutingCustomRuleTabKey();
}

void RoutingSettingsPageWidget::setupUi()
{
    auto* routingLayout = new QVBoxLayout(this);

    baseRouteTitleLabel_ = new QLabel(tr("Base Route"), this);
    baseRouteTitleLabel_->setObjectName(QStringLiteral("routingBaseRouteTitle"));
    AppTheme::applyCompactFont(baseRouteTitleLabel_);
    QFont baseRouteTitleFont = baseRouteTitleLabel_->font();
    baseRouteTitleFont.setBold(true);
    baseRouteTitleLabel_->setFont(baseRouteTitleFont);
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
    AppTheme::applyCompactFont(customRulesTitleLabel_);
    QFont customRulesTitleFont = customRulesTitleLabel_->font();
    customRulesTitleFont.setBold(true);
    customRulesTitleLabel_->setFont(customRulesTitleFont);
    routingLayout->addWidget(customRulesTitleLabel_);

    customRuleTabs_ = new QTabWidget(this);
    customRuleTabs_->setObjectName(QStringLiteral("routingCustomRuleTabs"));
    AppTheme::applyCompactFont(customRuleTabs_);
    customRuleTabs_->setDocumentMode(true);
    customRuleTabs_->tabBar()->setDrawBase(false);
    customRuleTabs_->tabBar()->setExpanding(false);
    AppTheme::applyCompactFont(customRuleTabs_->tabBar());

    const QList<QPair<QString, QString>> customTabs{
        {QStringLiteral("block"), QStringLiteral("Block")},
        {QStringLiteral("direct"), QStringLiteral("Direct")},
        {QStringLiteral("proxy"), QStringLiteral("Proxy")}};
    for (const auto& tab : customTabs) {
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

void RoutingSettingsPageWidget::reloadRoutingPresentation(int selectedRow)
{
    if (baseRouteLayout_ == nullptr || baseRouteButtonGroup_ == nullptr) {
        return;
    }

    const QList<QAbstractButton*> oldButtons = baseRouteButtonGroup_->buttons();
    for (QAbstractButton* button : oldButtons) {
        baseRouteButtonGroup_->removeButton(button);
    }
    clearLayout(baseRouteLayout_);

    int rowToSelect = selectedRow;
    if (rowToSelect < 0 || rowToSelect >= routingItems_.size()) {
        rowToSelect = routingItems_.isEmpty() ? -1 : 0;
    }

    for (int index = 0; index < routingItems_.size(); ++index) {
        auto* card = new BaseRouteCardButton(this);
        const QString collapsedText = baseRouteCardText(routingItems_.at(index), index, card->fontMetrics());
        const QString fullText = baseRouteCardFullText(routingItems_.at(index), index);
        card->setObjectName(QStringLiteral("routingBaseRouteCard_%1").arg(index));
        card->setCheckable(true);
        card->setProperty("collapsedText", collapsedText);
        card->setProperty("fullText", fullText);
        card->setProperty("baseRouteCard", true);
        card->setText(index == rowToSelect ? fullText : collapsedText);
        card->setToolTip(fullText);
        card->setChecked(index == rowToSelect);
        baseRouteButtonGroup_->addButton(card, index);
        baseRouteLayout_->addWidget(card, 0);
    }
    updateBaseRouteCardGeometry();
    updateRoutingActionState();
}

void RoutingSettingsPageWidget::updateBaseRouteCardGeometry()
{
    if (baseRouteButtonGroup_ == nullptr) {
        return;
    }

    const int selectedId = baseRouteButtonGroup_->checkedId();
    for (QAbstractButton* button : baseRouteButtonGroup_->buttons()) {
        if (button == nullptr) {
            continue;
        }

        const bool selected = baseRouteButtonGroup_->id(button) == selectedId;
        if (selected) {
            button->setMinimumWidth(kBaseRouteCardMaxWidth);
            button->setMaximumWidth(QWIDGETSIZE_MAX);
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            button->setText(button->property("fullText").toString());
        } else {
            button->setMinimumWidth(kBaseRouteCardCollapsedWidth);
            button->setMaximumWidth(kBaseRouteCardCollapsedWidth);
            button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
            button->setText(button->property("collapsedText").toString());
        }

        if (baseRouteLayout_ != nullptr) {
            const int layoutIndex = baseRouteLayout_->indexOf(button);
            if (layoutIndex >= 0) {
                baseRouteLayout_->setStretch(layoutIndex, selected ? 1 : 0);
            }
        }
    }

    if (baseRouteLayout_ != nullptr) {
        baseRouteLayout_->invalidate();
    }
}

void RoutingSettingsPageWidget::loadRoutingCustomRules(const QList<RoutingRule>& rules)
{
    for (auto it = customRuleEditors_.begin(); it != customRuleEditors_.end(); ++it) {
        if (it.value().protocolEdit != nullptr) {
            it.value().protocolEdit->clear();
        }
        if (it.value().portEdit != nullptr) {
            it.value().portEdit->clear();
        }
        if (it.value().ipEdit != nullptr) {
            it.value().ipEdit->clear();
        }
        if (it.value().domainEdit != nullptr) {
            it.value().domainEdit->clear();
        }
    }

    struct RuleValues {
        QStringList protocols;
        QStringList ports;
        QStringList ips;
        QStringList domains;
    };
    QMap<QString, RuleValues> valuesByAction;
    const auto appendUnique = [](QStringList& target, const QStringList& values) {
        for (const QString& value : values) {
            const QString trimmed = value.trimmed();
            if (!trimmed.isEmpty() && !target.contains(trimmed)) {
                target.append(trimmed);
            }
        }
    };

    for (const RoutingRule& rule : rules) {
        const QString action = rule.outboundTag.trimmed().toLower();
        if (!customRuleEditors_.contains(action)) {
            continue;
        }

        RuleValues& values = valuesByAction[action];
        appendUnique(values.protocols, rule.protocol);
        appendUnique(values.ips, rule.ip);
        appendUnique(values.domains, rule.domain);
        const QString port = rule.port.trimmed();
        if (!port.isEmpty() && !values.ports.contains(port)) {
            values.ports.append(port);
        }
    }

    for (auto it = valuesByAction.cbegin(); it != valuesByAction.cend(); ++it) {
        auto editorIt = customRuleEditors_.find(it.key());
        if (editorIt == customRuleEditors_.end()) {
            continue;
        }

        const RuleValues& values = it.value();
        CustomRuleEditors& editors = editorIt.value();
        if (editors.protocolEdit != nullptr) {
            editors.protocolEdit->setPlainText(values.protocols.join(QChar('\n')));
        }
        if (editors.portEdit != nullptr) {
            editors.portEdit->setText(joinValues(values.ports));
        }
        if (editors.ipEdit != nullptr) {
            editors.ipEdit->setPlainText(values.ips.join(QChar('\n')));
        }
        if (editors.domainEdit != nullptr) {
            editors.domainEdit->setPlainText(values.domains.join(QChar('\n')));
        }
    }
}

QList<RoutingItem> RoutingSettingsPageWidget::collectRoutingItems() const
{
    QList<RoutingItem> items = routingItems_;
    const int selectedIndex = selectedBaseRouteIndex();
    for (int index = 0; index < items.size(); ++index) {
        items[index].enabled = true;
        items[index].locked = index == selectedIndex;
    }
    return items;
}

QList<RoutingRule> RoutingSettingsPageWidget::collectRoutingCustomRules() const
{
    QList<RoutingRule> rules;
    const QStringList actionOrder{
        QStringLiteral("block"),
        QStringLiteral("direct"),
        QStringLiteral("proxy")};

    for (const QString& action : actionOrder) {
        const CustomRuleEditors editors = customRuleEditors_.value(action);

        const QString protocolText = editors.protocolEdit == nullptr ? QString() : editors.protocolEdit->toPlainText();
        const QString portText = editors.portEdit == nullptr ? QString() : editors.portEdit->text().trimmed();
        const QStringList protocols = splitValues(protocolText);
        if (!protocols.isEmpty() || !portText.isEmpty()) {
            RoutingRule rule;
            rule.type = QStringLiteral("field");
            rule.enabled = true;
            rule.outboundTag = action;
            rule.protocol = protocols;
            rule.port = portText;
            rules.append(rule);
        }

        const QStringList ips = editors.ipEdit == nullptr
            ? QStringList()
            : splitValues(editors.ipEdit->toPlainText());
        if (!ips.isEmpty()) {
            RoutingRule rule;
            rule.type = QStringLiteral("field");
            rule.enabled = true;
            rule.outboundTag = action;
            rule.ip = ips;
            rules.append(rule);
        }

        const QStringList domains = editors.domainEdit == nullptr
            ? QStringList()
            : splitValues(editors.domainEdit->toPlainText());
        if (!domains.isEmpty()) {
            RoutingRule rule;
            rule.type = QStringLiteral("field");
            rule.enabled = true;
            rule.outboundTag = action;
            rule.domain = domains;
            rules.append(rule);
        }
    }

    return rules;
}

void RoutingSettingsPageWidget::selectRoutingCustomRuleTab(const QString& key)
{
    if (customRuleTabs_ == nullptr) {
        return;
    }

    const QString normalizedKey = normalizedRoutingCustomRuleTabKey(key);
    for (int index = 0; index < customRuleTabs_->count(); ++index) {
        QWidget* tab = customRuleTabs_->widget(index);
        if (tab != nullptr && tab->property("routingCustomRuleTabKey").toString() == normalizedKey) {
            customRuleTabs_->setCurrentIndex(index);
            return;
        }
    }
}

QString RoutingSettingsPageWidget::selectedRoutingCustomRuleTabKey() const
{
    if (customRuleTabs_ == nullptr || customRuleTabs_->currentIndex() < 0) {
        return QStringLiteral("direct");
    }

    const QWidget* tab = customRuleTabs_->widget(customRuleTabs_->currentIndex());
    return normalizedRoutingCustomRuleTabKey(
        tab == nullptr ? QString() : tab->property("routingCustomRuleTabKey").toString());
}

void RoutingSettingsPageWidget::updateRoutingActionState()
{
    const bool hasBaseRoutes = !routingItems_.isEmpty();

    if (baseRouteTitleLabel_ != nullptr) {
        baseRouteTitleLabel_->setText(hasBaseRoutes
            ? tr("Base Route")
            : tr("Base Route (No Presets Configured)"));
        baseRouteTitleLabel_->setToolTip(hasBaseRoutes
            ? QString()
            : tr("No base routing preset is configured. Custom rules below are still applied."));
    }
    if (customRulesTitleLabel_ != nullptr) {
        customRulesTitleLabel_->setText(hasBaseRoutes
            ? tr("Custom Rules")
            : tr("Custom Rules (Still Applied)"));
        customRulesTitleLabel_->setToolTip(hasBaseRoutes
            ? tr("Custom rules are evaluated before the selected base route.")
            : tr("Custom rules are applied even when no base routing preset is configured."));
    }
    if (customRuleTabs_ != nullptr) {
        customRuleTabs_->setToolTip(customRulesTitleLabel_ == nullptr
            ? QString()
            : customRulesTitleLabel_->toolTip());
    }
}

int RoutingSettingsPageWidget::findInitialRouteIndex(const QList<RoutingItem>& items, const Config& config) const
{
    if (items.isEmpty()) {
        return -1;
    }

    if (config.enableRoutingAdvanced && config.routingIndex >= 0 && config.routingIndex < items.size()) {
        return config.routingIndex;
    }

    for (int index = 0; index < items.size(); ++index) {
        if (items.at(index).locked) {
            return index;
        }
    }

    for (int index = 0; index < items.size(); ++index) {
        if (items.at(index).enabled) {
            return index;
        }
    }

    return 0;
}

int RoutingSettingsPageWidget::selectedBaseRouteIndex() const
{
    if (baseRouteButtonGroup_ == nullptr) {
        return routingItems_.isEmpty() ? -1 : 0;
    }

    const int selectedId = baseRouteButtonGroup_->checkedId();
    if (selectedId >= 0) {
        return selectedId;
    }

    return routingItems_.isEmpty() ? -1 : 0;
}

QString RoutingSettingsPageWidget::joinValues(const QStringList& values)
{
    QStringList normalized;
    for (const QString& value : values) {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty()) {
            normalized.append(trimmed);
        }
    }
    return normalized.join(QStringLiteral(", "));
}

QStringList RoutingSettingsPageWidget::splitValues(const QString& value)
{
    QStringList values;
    const QStringList parts = value.split(QRegularExpression(QStringLiteral("[,\\n]")), Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty()) {
            values.append(trimmed);
        }
    }
    return values;
}
