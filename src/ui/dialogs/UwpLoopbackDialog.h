#pragma once

#include <QDialog>
#include <QHash>
#include <QList>
#include <QSet>

#include "platform/windows/WindowsUwpLoopbackService.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QShowEvent;
class QStackedLayout;
class QTableWidget;

class UwpLoopbackDialog final : public QDialog {
    Q_OBJECT

public:
    explicit UwpLoopbackDialog(QWidget* parent = nullptr);

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

    WindowsUwpLoopbackService loopbackService_;
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
