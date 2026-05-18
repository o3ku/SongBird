#pragma once

#include <QList>
#include <QMap>
#include <QWidget>

#include "domain/models/Config.h"
#include "domain/models/RoutingItem.h"
#include "domain/models/RoutingRule.h"

class QLabel;
class QButtonGroup;
class QHBoxLayout;
class QTabWidget;
class QLineEdit;
class QTextEdit;

class RoutingSettingsPageWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit RoutingSettingsPageWidget(QWidget* parent = nullptr);

    void setConfig(const Config& config);
    QList<RoutingItem> routingItems() const;
    QList<RoutingRule> routingCustomRules() const;
    QString settingsRoutingRuleTabKey() const;

private:
    struct CustomRuleEditors {
        QTextEdit* protocolEdit = nullptr;
        QLineEdit* portEdit = nullptr;
        QTextEdit* ipEdit = nullptr;
        QTextEdit* domainEdit = nullptr;
    };

    void setupUi();
    void reloadRoutingPresentation(int selectedRow = 0);
    void updateBaseRouteCardGeometry();
    void loadRoutingCustomRules(const QList<RoutingRule>& rules);
    QList<RoutingItem> collectRoutingItems() const;
    QList<RoutingRule> collectRoutingCustomRules() const;
    void selectRoutingCustomRuleTab(const QString& key);
    QString selectedRoutingCustomRuleTabKey() const;
    void updateRoutingActionState();
    int findInitialRouteIndex(const QList<RoutingItem>& items, const Config& config) const;
    int selectedBaseRouteIndex() const;
    static QString joinValues(const QStringList& values);
    static QStringList splitValues(const QString& value);

    QList<RoutingItem> routingItems_;
    QLabel* baseRouteTitleLabel_ = nullptr;
    QLabel* customRulesTitleLabel_ = nullptr;
    QButtonGroup* baseRouteButtonGroup_ = nullptr;
    QHBoxLayout* baseRouteLayout_ = nullptr;
    QTabWidget* customRuleTabs_ = nullptr;
    QMap<QString, CustomRuleEditors> customRuleEditors_;
    Config config_;
};
