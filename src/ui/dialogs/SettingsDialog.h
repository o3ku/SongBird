#pragma once

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QList>

#include "domain/models/Config.h"
#include "domain/models/CoreTypeItem.h"

class QCheckBox;
class QStackedLayout;
class QTabBar;
class QPushButton;
class QWidget;

#include "ui/dialogs/CoreSettingsPageWidget.h"
#include "ui/dialogs/DnsSettingsPageWidget.h"
#include "ui/dialogs/GeneralSettingsPageWidget.h"
#include "ui/dialogs/RoutingSettingsPageWidget.h"
#include "ui/dialogs/SubscriptionSettingsPageWidget.h"
#include "ui/dialogs/TunSettingsPageWidget.h"

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

    Config config_;
    GeneralSettingsPageWidget* generalSettingsPage_ = nullptr;
    TunSettingsPageWidget* tunSettingsPage_ = nullptr;
    QPushButton* restoreBackupButton_ = nullptr;
    QDialogButtonBox* buttonBox_ = nullptr;
    bool restoreBackupRequested_ = false;
    bool updateSubRequested_ = false;
    bool coreDownloadRequested_ = false;
    CoreType requestedCoreDownload_ = CoreType::Unknown;
    CoreType updatingCoreType_ = CoreType::Unknown;
    QTabBar* settingsTabBar_ = nullptr;
    QStackedLayout* settingsStackLayout_ = nullptr;
    int requestedTabIndex_ = 0;

    SubscriptionSettingsPageWidget* subscriptionSettingsPage_ = nullptr;
    RoutingSettingsPageWidget* routingSettingsPage_ = nullptr;
    CoreSettingsPageWidget* coreSettingsPage_ = nullptr;
    DnsSettingsPageWidget* dnsSettingsPage_ = nullptr;
};
