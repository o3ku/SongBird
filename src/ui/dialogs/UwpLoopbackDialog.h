#pragma once

#include <functional>
#include <memory>

#include <QDialog>
#include <QHash>
#include <QList>
#include <QSet>
#include <QStringList>

#include "common/OperationResult.h"
#include "platform/IUwpLoopbackService.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QShowEvent;
class QStackedLayout;
class QTableWidget;

class UwpLoopbackDialog final : public QDialog {
    Q_OBJECT

public:
    // Everything this dialog needs from outside itself.
    //
    // `loopbackService` is held by shared_ptr rather than the unique_ptr the other injected
    // services use: the dialog drives it from a worker thread, so the worker takes its own
    // reference instead of depending on the dialog outliving the call.
    //
    // `isProcessElevated` picks which of the two apply paths runs. It is a callback for the
    // same reason TunModeCoordinator's is: the branch cannot be reached from a test otherwise,
    // and the two branches do genuinely different things (one elevated batch versus one call
    // per package). Left empty it falls back to the real process check, so forgetting to set
    // it cannot silently select the wrong path.
    struct Dependencies {
        std::shared_ptr<IUwpLoopbackService> loopbackService;
        std::function<bool()> isProcessElevated;
    };

    explicit UwpLoopbackDialog(QWidget* parent = nullptr, Dependencies dependencies = {});

public slots:
    void reject() override;

protected:
    void showEvent(QShowEvent* event) override;

private:
    void setupUi();
    void startLoadingPackages();
    void finishLoadingPackages(QList<WindowsUwpPackageInfo> loadedPackages, const OperationResult& result);
    void setLoading(bool loading);
    void reloadTable();
    void applyFilter();
    void applyChanges();
    void updateActionState();
    void setStatus(const QString& statusText);
    void updateStatusSummary();
    bool confirmDiscardChanges();
    bool isPackageDirty(const QString& packageFamilyName) const;
    bool currentLoopbackState(const QString& packageFamilyName) const;
    bool isProcessElevated() const;

    Dependencies dependencies_;
    QList<WindowsUwpPackageInfo> packages_;
    QHash<QString, bool> originalEnabledByPackage_;
    QSet<QString> dirtyPackages_;
    QLineEdit* filterEdit_ = nullptr;
    QPushButton* refreshButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QStackedLayout* contentStack_ = nullptr;
    QTableWidget* table_ = nullptr;
    QLabel* loadingLabel_ = nullptr;
    QPushButton* applyButton_ = nullptr;
    QString statusMessage_;
    bool applying_ = false;
    bool loading_ = false;
    bool initialLoadStarted_ = false;
};
