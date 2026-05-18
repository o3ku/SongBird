#pragma once

#include <functional>

#include <QList>
#include <QMap>
#include <QString>
#include <QWidget>

#include "domain/models/Config.h"
#include "domain/models/CoreTypeItem.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QTabWidget;
class QVBoxLayout;

class CoreSettingsPageWidget final : public QWidget
{
public:
    explicit CoreSettingsPageWidget(QWidget* parent = nullptr);

    void setConfig(const Config& config);
    void setExistingCoreTypes(const QList<CoreType>& coreTypes);
    QList<CoreTypeItem> collectCoreTypeItems() const;
    bool enableCacheFile4Sbox() const;
    QString mux4SboxProtocol() const;
    bool legacyProtectEnabled() const;
    void setCoreVersion(CoreType coreType, const QString& version);
    void beginCoreUpdate(CoreType coreType);
    void setCoreUpdateProgress(CoreType coreType, const QString& message);
    void finishCoreUpdate(CoreType coreType, bool success, const QString& message = {});
    void setTunOptionsEnabled(bool enabled);
    void setCoreDownloadHandler(const std::function<void(CoreType)>& handler);

private:
    struct CoreProtocolEntry {
        QString name;
        ConfigType configType = ConfigType::VMess;
    };

    struct CoreStatusRow {
        QLabel* versionLabel = nullptr;
        QLabel* statusLabel = nullptr;
        QPushButton* downloadButton = nullptr;
        QPushButton* setAllButton = nullptr;
        QWidget* page = nullptr;
        QVBoxLayout* optionsLayout = nullptr;
        QString versionText;
        QString progressText;
    };

    void setupUi();
    void reloadCoreTypeTable();
    void refreshCoreStatusPresentation();
    QString coreStatusTextForProgress(const QString& message) const;

    Config config_;
    QCheckBox* enableCacheFile4SboxCheck_ = nullptr;
    QComboBox* mux4SboxProtocolCombo_ = nullptr;
    QCheckBox* tunEnableLegacyProtectCheck_ = nullptr;
    QList<CoreTypeItem> coreTypeItems_;
    QList<QComboBox*> coreTypeCombos_;
    QList<CoreProtocolEntry> coreProtocolEntries_;
    QList<CoreType> existingCoreTypes_;
    QTabWidget* coreDetailTabs_ = nullptr;
    QMap<int, CoreStatusRow> coreStatusRows_;
    CoreType updatingCoreType_ = CoreType::Unknown;
    std::function<void(CoreType)> coreDownloadHandler_;
};
