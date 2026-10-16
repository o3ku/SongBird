#pragma once

#include <functional>

#include <QList>
#include <QMap>
#include <QString>
#include <QWidget>

#include "domain/models/Config.h"
#include "domain/models/CoreTypeItem.h"
#include "ui/dialogs/CoreSettingsPageSupport.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QTabWidget;
class QVBoxLayout;

class CoreSettingsPageWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit CoreSettingsPageWidget(QWidget* parent = nullptr);

    void setConfig(const Config& config);
    void setExistingCoreTypes(const QList<CoreType>& coreTypes);
    QList<CoreTypeItem> collectCoreTypeItems() const;
    bool enableCacheFile4Sbox() const;
    QString mux4SboxProtocol() const;
    bool xrayFragmentEnabled() const;
    QString xrayDefaultUserAgent() const;
    bool legacyProtectEnabled() const;
    // sing-box-only DNS settings that used to sit on the DNS page. They are written
    // by the sing-box generator only, so they belong next to the other sing-box
    // options instead of appearing to apply to every core.
    QString proxyServerDns() const;
    bool blockBindingQuery() const;
    bool globalFakeIp() const;
    // Xray-only DNS settings, same reasoning: LegacyDnsConfigBuilder is the only
    // reader.
    bool serveStale() const;
    bool parallelQuery() const;
    QString directExpectedIps() const;
    // Enables/disables the sing-box "Global FakeIP" row from the DNS page's FakeIP
    // checkbox, which stayed shared.
    void setFakeIpEnabled(bool enabled);
    void setCoreVersion(CoreType coreType, const QString& version);
    void beginCoreUpdate(CoreType coreType);
    void setCoreUpdateProgress(CoreType coreType, const QString& message);
    void setCoreUpdateDownloadProgress(CoreType coreType, qint64 bytesReceived, qint64 bytesTotal);
    void finishCoreUpdate(CoreType coreType, bool success, const QString& message = {});
    // Marks a cancellation as in flight so the cancel button greys out until the
    // worker actually stops.
    void markCoreUpdateCancelRequested(CoreType coreType);
    void setTunOptionsEnabled(bool enabled);
    void setCoreDownloadHandler(const std::function<void(CoreType)>& handler);
    // Invoked when the user presses Cancel during an update.
    void setCoreUpdateCancelHandler(const std::function<void(CoreType)>& handler);

private:
    struct CoreStatusRow {
        QLabel* versionLabel = nullptr;
        QLabel* statusLabel = nullptr;
        QProgressBar* downloadProgress = nullptr;
        QPushButton* downloadButton = nullptr;
        QPushButton* cancelButton = nullptr;
        QPushButton* setAllButton = nullptr;
        QWidget* page = nullptr;
        QVBoxLayout* optionsLayout = nullptr;
        QString versionText;
        QString progressText;
        qint64 downloadReceivedBytes = 0;
        qint64 downloadTotalBytes = 0;
        bool downloadActive = false;
        // The reason the last attempt failed, shown on the retry button's tooltip.
        bool lastAttemptFailed = false;
        QString failureReason;
        // A cancel was requested and the worker has not stopped yet.
        bool cancelRequested = false;
    };

    void setupUi();
    void reloadCoreTypeTable();
    void refreshCoreStatusPresentation();
    // The shared label column for the "label + input" option rows, so their editors start at
    // the same x the way QFormLayout aligns the other settings pages.
    int coreOptionLabelWidthFor(const QStringList& labels) const;
    void alignCoreOptionLabel(QLabel* label) const;
    QCheckBox* enableCacheFile4SboxCheck_ = nullptr;
    QComboBox* mux4SboxProtocolCombo_ = nullptr;
    QCheckBox* xrayEnableFragmentCheck_ = nullptr;
    QComboBox* xrayDefaultUserAgentCombo_ = nullptr;
    QCheckBox* tunEnableLegacyProtectCheck_ = nullptr;
    QLineEdit* proxyServerDnsEdit_ = nullptr;
    QCheckBox* blockBindingQueryCheck_ = nullptr;
    QCheckBox* globalFakeIpCheck_ = nullptr;
    QCheckBox* serveStaleCheck_ = nullptr;
    QCheckBox* parallelQueryCheck_ = nullptr;
    QLineEdit* directExpectedIpsEdit_ = nullptr;
    QList<CoreTypeItem> coreTypeItems_;
    QList<QComboBox*> coreTypeCombos_;
    QList<CoreSettingsPageSupport::CoreProtocolEntry> coreProtocolEntries_;
    QList<CoreType> existingCoreTypes_;
    QTabWidget* coreDetailTabs_ = nullptr;
    // The per-protocol mapping table. Shown directly on the page; it used to be
    // hidden behind an "Advanced" toggle.
    QWidget* coreProtocolSection_ = nullptr;
    int coreOptionLabelWidth_ = 0;
    QMap<int, CoreStatusRow> coreStatusRows_;
    CoreType updatingCoreType_ = CoreType::Unknown;
    std::function<void(CoreType)> coreDownloadHandler_;
    std::function<void(CoreType)> coreUpdateCancelHandler_;
};
