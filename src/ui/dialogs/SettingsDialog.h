#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QList>

#include "domain/models/Config.h"

class QCheckBox;
class QDialogButtonBox;
class QStackedLayout;
class QTabBar;
class QPushButton;
class QWidget;
class CoreSettingsPageWidget;
class DnsSettingsPageWidget;
class GeneralSettingsPageWidget;
class RoutingSettingsPageWidget;
class SubscriptionSettingsPageWidget;
class TunSettingsPageWidget;

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
    int coreSettingsTabIndex() const;

    Config config_;
    GeneralSettingsPageWidget* generalSettingsPage_ = nullptr;
    TunSettingsPageWidget* tunSettingsPage_ = nullptr;
    QPushButton* restoreBackupButton_ = nullptr;
    QDialogButtonBox* buttonBox_ = nullptr;
    bool restoreBackupRequested_ = false;
    bool updateSubRequested_ = false;
    bool coreDownloadRequested_ = false;
    CoreType requestedCoreDownload_ = CoreType::Unknown;
    QTabBar* settingsTabBar_ = nullptr;
    QStackedLayout* settingsStackLayout_ = nullptr;
    int requestedTabIndex_ = 0;

    SubscriptionSettingsPageWidget* subscriptionSettingsPage_ = nullptr;
    RoutingSettingsPageWidget* routingSettingsPage_ = nullptr;
    CoreSettingsPageWidget* coreSettingsPage_ = nullptr;
    DnsSettingsPageWidget* dnsSettingsPage_ = nullptr;
};
