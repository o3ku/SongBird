#include "ui/dialogs/RoutingSettingsPageWidget.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTabWidget>
#include <QTextEdit>

#include "ui/dialogs/RoutingBaseRouteCard.h"
#include "ui/dialogs/RoutingCustomRuleSupport.h"
#include "domain/models/RoutingProfiles.h"

namespace {

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
    routingItems_ = RoutingProfiles::routingItems(config.collection());
    reloadRoutingPresentation(findInitialRouteIndex(routingItems_, config.collection().routingModeId));
    loadRoutingCustomRules(config.collection().routingCustomRules);
    selectRoutingCustomRuleTab(config.ui().settingsRoutingRuleTabKey);
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
        auto* card = new RoutingBaseRouteCardButton(this);
        const QString collapsedText =
            RoutingBaseRouteCard::collapsedText(routingItems_.at(index), index, card->fontMetrics());
        const QString fullText = RoutingBaseRouteCard::fullText(routingItems_.at(index), index);
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
    RoutingBaseRouteCard::applyGeometry(baseRouteButtonGroup_, baseRouteLayout_);
}

void RoutingSettingsPageWidget::loadRoutingCustomRules(const QList<RoutingRule>& rules)
{
    preservedCustomRules_.clear();

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

    const RoutingCustomRuleSupport::PartitionedRules partitioned =
        RoutingCustomRuleSupport::partitionEditableRules(rules, customRuleEditors_.keys());
    preservedCustomRules_ = partitioned.preservedRules;

    for (auto it = partitioned.valuesByAction.cbegin(); it != partitioned.valuesByAction.cend(); ++it) {
        auto editorIt = customRuleEditors_.find(it.key());
        if (editorIt == customRuleEditors_.end()) {
            continue;
        }

        const RoutingCustomRuleSupport::RuleValues& values = it.value();
        CustomRuleEditors& editors = editorIt.value();
        if (editors.protocolEdit != nullptr) {
            editors.protocolEdit->setPlainText(values.protocols.join(QChar('\n')));
        }
        if (editors.portEdit != nullptr) {
            editors.portEdit->setText(RoutingCustomRuleSupport::joinValues(values.ports));
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
    QMap<QString, RoutingCustomRuleSupport::RuleValues> valuesByAction;
    for (const QString& action : RoutingCustomRuleSupport::actionOrder()) {
        const CustomRuleEditors editors = customRuleEditors_.value(action);
        RoutingCustomRuleSupport::RuleValues& values = valuesByAction[action];

        const QString protocolText = editors.protocolEdit == nullptr ? QString() : editors.protocolEdit->toPlainText();
        const QString portText = editors.portEdit == nullptr ? QString() : editors.portEdit->text().trimmed();
        values.protocols = RoutingCustomRuleSupport::splitValues(protocolText);
        if (!portText.isEmpty()) {
            values.ports = QStringList{portText};
        }

        values.ips = editors.ipEdit == nullptr
            ? QStringList()
            : RoutingCustomRuleSupport::splitValues(editors.ipEdit->toPlainText());

        values.domains = editors.domainEdit == nullptr
            ? QStringList()
            : RoutingCustomRuleSupport::splitValues(editors.domainEdit->toPlainText());
    }

    return RoutingCustomRuleSupport::collectRules(preservedCustomRules_, valuesByAction);
}

void RoutingSettingsPageWidget::selectRoutingCustomRuleTab(const QString& key)
{
    if (customRuleTabs_ == nullptr) {
        return;
    }

    const QString normalizedKey = RoutingCustomRuleSupport::normalizedTabKey(key);
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
    return RoutingCustomRuleSupport::normalizedTabKey(
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

int RoutingSettingsPageWidget::findInitialRouteIndex(const QList<RoutingItem>& items, const QString& selectedRoutingModeId) const
{
    if (items.isEmpty()) {
        return -1;
    }

    const QString routingModeId = selectedRoutingModeId.trimmed();
    if (!routingModeId.isEmpty()) {
        for (int index = 0; index < items.size(); ++index) {
            if (items.at(index).id == routingModeId) {
                return index;
            }
        }
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
