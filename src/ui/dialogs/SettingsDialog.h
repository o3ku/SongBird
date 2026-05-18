#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMap>
#include <QSpinBox>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextEdit>

#include "domain/models/Config.h"
#include "domain/models/CoreTypeItem.h"

#include "domain/models/RoutingItem.h"
#include "domain/models/SubItem.h"

class QButtonGroup;
class QHBoxLayout;
class QStackedLayout;
class QTabBar;
class QPushButton;
class QWidget;
class QVBoxLayout;

#include "ui/dialogs/CoreSettingsPageWidget.h"
#include "ui/dialogs/SubscriptionSettingsPageWidget.h"

class SettingsDialog final : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    void setConfig(const Config& config);
    Config config() const;
    bool wasRestoreBackupRequested() const { return restoreBackupRequested_; }
    bool wasUpdateSubRequested() const { return updateSubRequested_; }
    bool wasCoreDownloadRequested() const { return coreDownloadRequested_; }
    CoreType requestedCoreDownload() const { return requestedCoreDownload_; }
    QList<int> selectedSubRows() const;
    void clearUpdateSubRequested() { updateSubRequested_ = false; }
    void selectTab(int index) { requestedTabIndex_ = index; }
    void setExistingCoreTypes(const QList<CoreType>& coreTypes);
    void setCoreVersion(CoreType coreType, const QString& version);
    void beginCoreUpdate(CoreType coreType);
    void setCoreUpdateProgress(CoreType coreType, const QString& message);
    void finishCoreUpdate(CoreType coreType, bool success, const QString& message = {});

signals:
    void coreDownloadRequested(int coreTypeValue);

private:
    void setupUi();
    void updateFieldState();
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

    Config config_;
    QCheckBox* showMainOnStartupCheck_ = nullptr;
    QCheckBox* autoRunCheck_ = nullptr;
    QCheckBox* allowLanConnectionCheck_ = nullptr;
    QCheckBox* sniffingEnabledCheck_ = nullptr;
    QCheckBox* routeOnlyCheck_ = nullptr;
    QSpinBox* localPortSpin_ = nullptr;
    QCheckBox* enableFragmentCheck_ = nullptr;
    QCheckBox* defaultAllowInsecureCheck_ = nullptr;
    QComboBox* defaultFingerprintCombo_ = nullptr;
    QComboBox* defaultUserAgentCombo_ = nullptr;
    QCheckBox* enableStatisticsCheck_ = nullptr;
    QSpinBox* statisticsFreshRateSpin_ = nullptr;
    QSpinBox* trayMenuServersLimitSpin_ = nullptr;
    QLineEdit* systemProxyExceptionsEdit_ = nullptr;
    QComboBox* systemProxyAdvancedProtocolCombo_ = nullptr;
    QLineEdit* pacUrlEdit_ = nullptr;
    QCheckBox* checkPreReleaseUpdateCheck_ = nullptr;
    QCheckBox* ignoreGeoUpdateCoreCheck_ = nullptr;
    QComboBox* languageCombo_ = nullptr;
    QCheckBox* tunEnableCheck_ = nullptr;
    QCheckBox* tunAutoRouteCheck_ = nullptr;
    QCheckBox* tunStrictRouteCheck_ = nullptr;
    QSpinBox* tunMtuSpin_ = nullptr;
    QComboBox* tunStackCombo_ = nullptr;
    QCheckBox* tunEnableIPv6AddressCheck_ = nullptr;
    QLineEdit* tunIcmpRoutingEdit_ = nullptr;
    // DNS tab
    QLineEdit* remoteDnsEdit_ = nullptr;
    QLineEdit* directDnsEdit_ = nullptr;
    QLineEdit* bootstrapDnsEdit_ = nullptr;
    QComboBox* domainStrategyForFreedomCombo_ = nullptr;
    QComboBox* domainStrategyForProxyCombo_ = nullptr;
    QCheckBox* useSystemHostsCheck_ = nullptr;
    QCheckBox* addCommonHostsCheck_ = nullptr;
    QCheckBox* blockBindingQueryCheck_ = nullptr;
    QCheckBox* fakeIpCheck_ = nullptr;
    QCheckBox* globalFakeIpCheck_ = nullptr;
    QCheckBox* serveStaleCheck_ = nullptr;
    QCheckBox* parallelQueryCheck_ = nullptr;
    QLineEdit* directExpectedIpsEdit_ = nullptr;
    QTextEdit* dnsHostsEdit_ = nullptr;
    QPushButton* restoreBackupButton_ = nullptr;
    QDialogButtonBox* buttonBox_ = nullptr;
    bool restoreBackupRequested_ = false;
    bool updateSubRequested_ = false;
    bool coreDownloadRequested_ = false;
    CoreType requestedCoreDownload_ = CoreType::Unknown;
    CoreType updatingCoreType_ = CoreType::Unknown;

    QList<RoutingItem> routingItems_;
    QLabel* baseRouteTitleLabel_ = nullptr;
    QLabel* customRulesTitleLabel_ = nullptr;
    QButtonGroup* baseRouteButtonGroup_ = nullptr;
    QHBoxLayout* baseRouteLayout_ = nullptr;
    QTabWidget* customRuleTabs_ = nullptr;
    struct CustomRuleEditors {
        QTextEdit* protocolEdit = nullptr;
        QLineEdit* portEdit = nullptr;
        QTextEdit* ipEdit = nullptr;
        QTextEdit* domainEdit = nullptr;
    };
    QMap<QString, CustomRuleEditors> customRuleEditors_;
    QTabBar* settingsTabBar_ = nullptr;
    QStackedLayout* settingsStackLayout_ = nullptr;
    int requestedTabIndex_ = 0;

    SubscriptionSettingsPageWidget* subscriptionSettingsPage_ = nullptr;
    CoreSettingsPageWidget* coreSettingsPage_ = nullptr;

};
