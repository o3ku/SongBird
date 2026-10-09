#include "ui/dialogs/UwpLoopbackDialog.h"

#include <utility>

#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QPointer>
#include <QPushButton>
#include <QShowEvent>
#include <QStackedLayout>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include "common/AppPlatform.h"
#include "common/BackgroundThreadLaunch.h"
#include "common/DialogUtils.h"
#include "ui/dialogs/UwpLoopbackDialogSupport.h"
#include "ui/theme/AppTheme.h"

namespace UwpUi = UwpLoopbackDialogSupport;

UwpLoopbackDialog::UwpLoopbackDialog(QWidget* parent, Dependencies dependencies)
    : QDialog(parent)
    , dependencies_(std::move(dependencies))
{
    // An unset elevation callback means the real process check; see the header for why the
    // fallback is the real thing rather than a constant.
    if (!dependencies_.isProcessElevated) {
        dependencies_.isProcessElevated = []() { return ::isProcessElevated(); };
    }
    Q_ASSERT(dependencies_.loopbackService != nullptr);

    setupUi();
}

bool UwpLoopbackDialog::isProcessElevated() const
{
    return dependencies_.isProcessElevated && dependencies_.isProcessElevated();
}

void UwpLoopbackDialog::reject()
{
    if (confirmDiscardChanges()) {
        QDialog::reject();
    }
}

void UwpLoopbackDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    if (initialLoadStarted_) {
        return;
    }

    initialLoadStarted_ = true;
    QTimer::singleShot(0, this, &UwpLoopbackDialog::startLoadingPackages);
}

void UwpLoopbackDialog::setupUi()
{
    setWindowTitle(tr("UWP Loopback"));
    resize(760, 520);
    AppTheme::applyCompactFont(this);

    filterEdit_ = new QLineEdit(this);
    filterEdit_->setObjectName(QStringLiteral("uwpLoopbackFilterEdit"));
    filterEdit_->setPlaceholderText(tr("Filter UWP apps"));

    refreshButton_ = new QPushButton(tr("Reload"), this);
    refreshButton_->setObjectName(QStringLiteral("uwpLoopbackRefreshButton"));

    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName(QStringLiteral("uwpLoopbackStatusLabel"));
    statusLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    statusLabel_->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    AppTheme::applyCompactFont(statusLabel_);

    applyButton_ = new QPushButton(tr("Apply"), this);
    applyButton_->setObjectName(QStringLiteral("uwpLoopbackApplyButton"));
    AppTheme::applyCompactFont({refreshButton_, applyButton_});

    auto* topLayout = new QHBoxLayout();
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(10);
    topLayout->addWidget(filterEdit_, 1);
    topLayout->addStretch(1);
    topLayout->addWidget(statusLabel_);
    topLayout->addWidget(refreshButton_);
    topLayout->addWidget(applyButton_);

    table_ = new QTableWidget(this);
    table_->setObjectName(QStringLiteral("uwpLoopbackTable"));
    UwpUi::configurePackageTable(table_, {tr("Enabled"), tr("App"), tr("Package Family Name")});

    loadingLabel_ = new QLabel(tr("Loading UWP app list..."), this);
    loadingLabel_->setObjectName(QStringLiteral("uwpLoopbackLoadingLabel"));
    loadingLabel_->setAlignment(Qt::AlignCenter);
    loadingLabel_->setProperty("sectionBody", true);
    AppTheme::applyCompactFont(loadingLabel_);

    contentStack_ = new QStackedLayout();
    contentStack_->setContentsMargins(0, 0, 0, 0);
    contentStack_->addWidget(table_);
    contentStack_->addWidget(loadingLabel_);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(16, 16, 16, 14);
    rootLayout->setSpacing(10);
    rootLayout->addLayout(topLayout);
    rootLayout->addLayout(contentStack_, 1);

    connect(filterEdit_, &QLineEdit::textChanged, this, &UwpLoopbackDialog::applyFilter);
    connect(refreshButton_, &QPushButton::clicked, this, [this]() {
        if (!loading_ && confirmDiscardChanges()) {
            startLoadingPackages();
        }
    });
    connect(applyButton_, &QPushButton::clicked, this, &UwpLoopbackDialog::applyChanges);
    connect(table_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (applying_ || item == nullptr || item->column() != UwpUi::EnabledColumn) {
            return;
        }

        const QString packageFamilyName = item->data(UwpUi::PackageRole).toString();
        if (packageFamilyName.trimmed().isEmpty()) {
            return;
        }

        for (WindowsUwpPackageInfo& package : packages_) {
            if (package.packageFamilyName == packageFamilyName) {
                package.loopbackEnabled = item->checkState() == Qt::Checked;
                break;
            }
        }

        if (isPackageDirty(packageFamilyName)) {
            dirtyPackages_.insert(packageFamilyName);
        } else {
            dirtyPackages_.remove(packageFamilyName);
        }
        updateActionState();
        updateStatusSummary();
    });
    updateActionState();
}

void UwpLoopbackDialog::startLoadingPackages()
{
    if (loading_) {
        return;
    }

    if (!dependencies_.loopbackService || !dependencies_.loopbackService->isAvailable()) {
        packages_.clear();
        originalEnabledByPackage_.clear();
        dirtyPackages_.clear();
        reloadTable();
        setStatus(tr("CheckNetIsolation.exe was not found."));
        updateActionState();
        return;
    }

    setLoading(true);
    setStatus(tr("Loading UWP app list..."));

    QPointer<UwpLoopbackDialog> dialogGuard(this);
    // The worker keeps its own reference: the dialog may be closed while the list is being
    // read, and the service must outlive the call either way.
    const std::shared_ptr<IUwpLoopbackService> service = dependencies_.loopbackService;
    QThread* thread = launchBackgroundThread([dialogGuard, service]() {
        OperationResult result;
        QList<WindowsUwpPackageInfo> loadedPackages = service->listPackages(&result);

        if (dialogGuard.isNull()) {
            return;
        }

        QMetaObject::invokeMethod(
            dialogGuard.data(),
            [dialogGuard, loadedPackages = std::move(loadedPackages), result]() mutable {
                if (!dialogGuard.isNull()) {
                    dialogGuard->finishLoadingPackages(std::move(loadedPackages), result);
                }
            },
            Qt::QueuedConnection);
    });
    thread->start();
}

void UwpLoopbackDialog::finishLoadingPackages(
    QList<WindowsUwpPackageInfo> loadedPackages,
    const OperationResult& result)
{
    setLoading(false);
    if (!result.success) {
        packages_.clear();
        originalEnabledByPackage_.clear();
        dirtyPackages_.clear();
        reloadTable();
        setStatus(result.message);
        updateActionState();
        return;
    }

    packages_ = std::move(loadedPackages);
    originalEnabledByPackage_.clear();
    dirtyPackages_.clear();
    for (const WindowsUwpPackageInfo& package : packages_) {
        originalEnabledByPackage_.insert(package.packageFamilyName, package.loopbackEnabled);
    }

    reloadTable();
    setStatus(isProcessElevated()
            ? tr("Loaded %1 UWP apps.").arg(packages_.size())
            : tr("Loaded %1 UWP apps. Applying changes will request administrator approval.").arg(packages_.size()));
    updateActionState();
}

void UwpLoopbackDialog::setLoading(bool loading)
{
    loading_ = loading;
    if (contentStack_ != nullptr) {
        contentStack_->setCurrentWidget(loading
                ? static_cast<QWidget*>(loadingLabel_)
                : static_cast<QWidget*>(table_));
    }
    if (filterEdit_ != nullptr) {
        filterEdit_->setEnabled(!loading);
    }
    updateActionState();
}

void UwpLoopbackDialog::reloadTable()
{
    UwpUi::populatePackageTable(table_, packages_);
    applyFilter();
}

void UwpLoopbackDialog::applyFilter()
{
    const QString needle = filterEdit_ == nullptr ? QString() : filterEdit_->text().trimmed();
    UwpUi::applyPackageFilter(table_, needle);
    updateStatusSummary();
}

void UwpLoopbackDialog::applyChanges()
{
    if (applying_ || dirtyPackages_.isEmpty() || !dependencies_.loopbackService) {
        return;
    }

    // Snapshot before handing the work off. The table stays live while the worker runs, so the
    // worker is given the packages and states this click was made against rather than reading
    // the dialog's own members from another thread.
    ApplyRequest request;
    request.elevated = isProcessElevated();
    for (const QString& packageFamilyName : dirtyPackages_) {
        request.requestedStates.insert(packageFamilyName, currentLoopbackState(packageFamilyName));
    }

    applying_ = true;
    updateActionState();

    QPointer<UwpLoopbackDialog> dialogGuard(this);
    const std::shared_ptr<IUwpLoopbackService> service = dependencies_.loopbackService;
    QThread* thread = launchBackgroundThread([dialogGuard, service, request]() {
        ApplyOutcome outcome;
        if (request.elevated) {
            for (auto it = request.requestedStates.constBegin();
                 it != request.requestedStates.constEnd();
                 ++it) {
                const OperationResult result = service->setLoopbackEnabled(it.key(), it.value());
                if (result.success) {
                    outcome.appliedStates.insert(it.key(), it.value());
                } else {
                    outcome.failures.append(QStringLiteral("%1: %2").arg(it.key(), result.message));
                }
            }
        } else {
            const OperationResult result =
                service->setLoopbackEnabledElevated(request.requestedStates);
            if (result.success) {
                outcome.appliedStates = request.requestedStates;
            } else {
                outcome.failures.append(result.message);
            }
        }

        if (dialogGuard.isNull()) {
            return;
        }

        QMetaObject::invokeMethod(
            dialogGuard.data(),
            [dialogGuard, outcome]() {
                if (!dialogGuard.isNull()) {
                    dialogGuard->finishApplyingChanges(outcome);
                }
            },
            Qt::QueuedConnection);
    });
    thread->start();
}

void UwpLoopbackDialog::finishApplyingChanges(const ApplyOutcome& outcome)
{
    applying_ = false;

    for (auto it = outcome.appliedStates.constBegin(); it != outcome.appliedStates.constEnd(); ++it) {
        originalEnabledByPackage_.insert(it.key(), it.value());
        dirtyPackages_.remove(it.key());
    }

    if (outcome.failures.isEmpty()) {
        setStatus(tr("Applied UWP loopback changes."));
        startLoadingPackages();
    } else {
        reloadTable();
        setStatus(tr("Some UWP loopback changes failed:\n%1").arg(outcome.failures.join(QChar('\n'))));
    }
    updateActionState();
}

void UwpLoopbackDialog::updateActionState()
{
    if (applyButton_ != nullptr) {
        applyButton_->setEnabled(!loading_ && !applying_ && !dirtyPackages_.isEmpty());
    }
    if (refreshButton_ != nullptr) {
        refreshButton_->setEnabled(!loading_ && !applying_);
    }
    if (table_ != nullptr) {
        // The apply runs on a worker thread, so the user can still reach the table while it is in
        // flight. The worker works from a snapshot, so an edit made now would be dropped by the
        // completion handler -- freeze the table instead of silently discarding the edit.
        table_->setEnabled(!loading_ && !applying_);
    }
    updateStatusSummary();
}

void UwpLoopbackDialog::setStatus(const QString& statusText)
{
    statusMessage_ = statusText.trimmed();
    updateStatusSummary();
}

void UwpLoopbackDialog::updateStatusSummary()
{
    if (statusLabel_ == nullptr) {
        return;
    }

    if (applying_) {
        // The apply can take minutes: an un-elevated one waits for the elevated helper script,
        // and an elevated one makes a process call per package. Say so in the label the user is
        // already looking at rather than only in its tooltip.
        const QString applyingText = tr("Applying UWP loopback changes...");
        statusLabel_->setText(applyingText);
        statusLabel_->setToolTip(applyingText);
        return;
    }

    const int selectedCount = UwpUi::enabledPackageCount(packages_);

    const QString summary = dirtyPackages_.isEmpty()
        ? tr("Enabled: %1/%2 apps").arg(selectedCount).arg(packages_.size())
        : tr("Enabled: %1/%2 apps, %3 pending")
              .arg(selectedCount)
              .arg(packages_.size())
              .arg(dirtyPackages_.size());
    statusLabel_->setText(summary);
    statusLabel_->setToolTip(statusMessage_);
}

bool UwpLoopbackDialog::confirmDiscardChanges()
{
    if (dirtyPackages_.isEmpty()) {
        return true;
    }

    return DialogUtils::askYesNoQuestion(
            this,
            tr("Discard Changes"),
            tr("Discard unapplied UWP loopback changes?"),
            QMessageBox::No)
        == QMessageBox::Yes;
}

bool UwpLoopbackDialog::isPackageDirty(const QString& packageFamilyName) const
{
    return originalEnabledByPackage_.value(packageFamilyName, false) != currentLoopbackState(packageFamilyName);
}

bool UwpLoopbackDialog::currentLoopbackState(const QString& packageFamilyName) const
{
    for (const WindowsUwpPackageInfo& package : packages_) {
        if (package.packageFamilyName == packageFamilyName) {
            return package.loopbackEnabled;
        }
    }
    return false;
}
