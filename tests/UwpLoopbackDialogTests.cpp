#include <QtTest>

#include <QHash>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>

#include <memory>

#include "platform/IUwpLoopbackService.h"
#include "ui/dialogs/UwpLoopbackDialog.h"
#include "ui/dialogs/UwpLoopbackDialogSupport.h"

namespace {

WindowsUwpPackageInfo makePackage(const QString& familyName, bool enabled)
{
    WindowsUwpPackageInfo package;
    package.name = familyName.section(QChar('_'), 0, 0);
    package.packageFamilyName = familyName;
    package.packageFullName = familyName + QStringLiteral("_1.0.0.0_x64__test");
    package.publisher = QStringLiteral("CN=Test");
    package.installLocation = QStringLiteral("C:/Program Files/WindowsApps/") + familyName;
    package.loopbackEnabled = enabled;
    return package;
}

// Stand-in for the CheckNetIsolation.exe side effect.
//
// The dialog used to construct WindowsUwpLoopbackService itself, which made every branch that
// depends on the service unreachable from a test: the machine's real package list came back
// instead of a fixture, and there was no way to make a call fail. This records what it was
// asked for so the dialog's routing -- one elevated batch versus one call per package -- and
// its failure handling can be asserted.
class FakeUwpLoopbackService final : public IUwpLoopbackService {
public:
    bool isAvailable() const override
    {
        return available;
    }

    QList<WindowsUwpPackageInfo> listPackages(OperationResult* result) const override
    {
        ++listPackagesCalls;
        if (result != nullptr) {
            *result = listResult;
        }
        return packages;
    }

    OperationResult setLoopbackEnabled(const QString& packageFamilyName, bool enabled) const override
    {
        perPackageRequests.append(qMakePair(packageFamilyName, enabled));
        return failingPackages.contains(packageFamilyName)
            ? OperationResult::fail(QStringLiteral("access denied"))
            : OperationResult::ok();
    }

    OperationResult setLoopbackEnabledElevated(const QHash<QString, bool>& requested) const override
    {
        elevatedRequests.append(requested);
        return elevatedResult;
    }

    bool available = true;
    OperationResult listResult = OperationResult::ok();
    QList<WindowsUwpPackageInfo> packages;
    // The per-package calls for these family names fail; every other one succeeds.
    QStringList failingPackages;
    OperationResult elevatedResult = OperationResult::ok();

    mutable int listPackagesCalls = 0;
    mutable QList<QPair<QString, bool>> perPackageRequests;
    mutable QList<QHash<QString, bool>> elevatedRequests;
};

QTableWidget* tableOf(const UwpLoopbackDialog& dialog)
{
    return dialog.findChild<QTableWidget*>(QStringLiteral("uwpLoopbackTable"));
}

QPushButton* applyButtonOf(const UwpLoopbackDialog& dialog)
{
    return dialog.findChild<QPushButton*>(QStringLiteral("uwpLoopbackApplyButton"));
}

QLabel* statusLabelOf(const UwpLoopbackDialog& dialog)
{
    return dialog.findChild<QLabel*>(QStringLiteral("uwpLoopbackStatusLabel"));
}

// The label's visible text is the "Enabled: x/y" summary; the message the dialog was last told
// to show lives in its tooltip. Assertions on messages therefore go through the tooltip.
QString statusMessageOf(const UwpLoopbackDialog& dialog)
{
    QLabel* label = statusLabelOf(dialog);
    return label == nullptr ? QString() : label->toolTip();
}

std::unique_ptr<UwpLoopbackDialog> makeDialog(
    const std::shared_ptr<FakeUwpLoopbackService>& service,
    bool elevated)
{
    UwpLoopbackDialog::Dependencies dependencies;
    dependencies.loopbackService = service;
    dependencies.isProcessElevated = [elevated]() { return elevated; };
    return std::make_unique<UwpLoopbackDialog>(nullptr, std::move(dependencies));
}

// Toggling the row is what marks it pending -- the dialog learns about the edit through
// QTableWidget::itemChanged, so the check state must be set on the item rather than on the
// dialog's own copy of the package list.
//
// Deliberately not a QVERIFY: an assertion inside a helper returns from the helper, not from the
// test, so a helper that silently gives up would let the test pass. Callers assert the row count
// themselves before calling this.
void setRowChecked(const UwpLoopbackDialog& dialog, int row, bool checked)
{
    QTableWidgetItem* item = tableOf(dialog)->item(row, UwpLoopbackDialogSupport::EnabledColumn);
    Q_ASSERT(item != nullptr);
    item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
}

} // namespace

class UwpLoopbackDialogTests : public QObject {
    Q_OBJECT

private slots:
    void packageListComesFromTheInjectedService();
    void unavailableServiceIsReportedAndNothingIsListed();
    void failedPackageListIsReported();
    void unelevatedApplySendsOneBatchToTheService();
    void elevatedApplySendsOneCallPerPackage();
    void failedUnelevatedBatchKeepsTheRowsPending();
    void failedPerPackageApplyNamesThePackage();
    void partialPerPackageFailureOnlyKeepsTheFailedRowPending();
    void successfulPerPackageApplyClearsThePendingRow();
};

void UwpLoopbackDialogTests::packageListComesFromTheInjectedService()
{
    auto service = std::make_shared<FakeUwpLoopbackService>();
    service->packages = {makePackage(QStringLiteral("Alpha_pf"), false),
                         makePackage(QStringLiteral("Beta_pf"), true)};
    const std::unique_ptr<UwpLoopbackDialog> dialog = makeDialog(service, true);
    dialog->show();

    QTRY_COMPARE_WITH_TIMEOUT(tableOf(*dialog)->rowCount(), 2, 10000);
    QCOMPARE(service->listPackagesCalls, 1);
    QCOMPARE(tableOf(*dialog)->item(0, UwpLoopbackDialogSupport::EnabledColumn)->checkState(),
             Qt::Unchecked);
    QCOMPARE(tableOf(*dialog)->item(1, UwpLoopbackDialogSupport::EnabledColumn)->checkState(),
             Qt::Checked);
}

void UwpLoopbackDialogTests::unavailableServiceIsReportedAndNothingIsListed()
{
    auto service = std::make_shared<FakeUwpLoopbackService>();
    service->available = false;
    service->packages = {makePackage(QStringLiteral("Alpha_pf"), false)};
    const std::unique_ptr<UwpLoopbackDialog> dialog = makeDialog(service, true);
    dialog->show();

    QTRY_VERIFY_WITH_TIMEOUT(
        statusMessageOf(*dialog).contains(QStringLiteral("CheckNetIsolation.exe was not found.")),
        10000);
    QCOMPARE(tableOf(*dialog)->rowCount(), 0);
    QCOMPARE(service->listPackagesCalls, 0);
    QVERIFY(!applyButtonOf(*dialog)->isEnabled());
}

void UwpLoopbackDialogTests::failedPackageListIsReported()
{
    auto service = std::make_shared<FakeUwpLoopbackService>();
    service->listResult = OperationResult::fail(QStringLiteral("Get-AppxPackage failed"));
    service->packages = {makePackage(QStringLiteral("Alpha_pf"), false)};
    const std::unique_ptr<UwpLoopbackDialog> dialog = makeDialog(service, true);
    dialog->show();

    QTRY_VERIFY_WITH_TIMEOUT(
        statusMessageOf(*dialog).contains(QStringLiteral("Get-AppxPackage failed")), 10000);
    QCOMPARE(tableOf(*dialog)->rowCount(), 0);
    QVERIFY(!applyButtonOf(*dialog)->isEnabled());
}

void UwpLoopbackDialogTests::unelevatedApplySendsOneBatchToTheService()
{
    auto service = std::make_shared<FakeUwpLoopbackService>();
    service->packages = {makePackage(QStringLiteral("Alpha_pf"), false),
                         makePackage(QStringLiteral("Beta_pf"), false)};
    const std::unique_ptr<UwpLoopbackDialog> dialog = makeDialog(service, false);
    dialog->show();
    QTRY_COMPARE_WITH_TIMEOUT(tableOf(*dialog)->rowCount(), 2, 10000);

    setRowChecked(*dialog, 0, true);
    setRowChecked(*dialog, 1, true);
    QVERIFY(applyButtonOf(*dialog)->isEnabled());
    applyButtonOf(*dialog)->click();

    // One elevated helper run covering every pending package, never one call per package: the
    // elevated path exists precisely to avoid a UAC prompt per row.
    QTRY_COMPARE_WITH_TIMEOUT(service->elevatedRequests.size(), 1, 10000);
    QCOMPARE(service->perPackageRequests.size(), 0);
    const QHash<QString, bool> requested = service->elevatedRequests.constFirst();
    QCOMPARE(requested.size(), 2);
    QVERIFY(requested.value(QStringLiteral("Alpha_pf")));
    QVERIFY(requested.value(QStringLiteral("Beta_pf")));

    // The pending set is committed, so the button goes back to disabled, and the dialog reloads
    // the package list to pick up the state it just wrote.
    QTRY_VERIFY_WITH_TIMEOUT(!applyButtonOf(*dialog)->isEnabled(), 10000);
    QTRY_COMPARE_WITH_TIMEOUT(service->listPackagesCalls, 2, 10000);
}

void UwpLoopbackDialogTests::elevatedApplySendsOneCallPerPackage()
{
    auto service = std::make_shared<FakeUwpLoopbackService>();
    service->packages = {makePackage(QStringLiteral("Alpha_pf"), false),
                         makePackage(QStringLiteral("Beta_pf"), true)};
    const std::unique_ptr<UwpLoopbackDialog> dialog = makeDialog(service, true);
    dialog->show();
    QTRY_COMPARE_WITH_TIMEOUT(tableOf(*dialog)->rowCount(), 2, 10000);

    setRowChecked(*dialog, 0, true);
    setRowChecked(*dialog, 1, false);
    applyButtonOf(*dialog)->click();

    QTRY_COMPARE_WITH_TIMEOUT(service->perPackageRequests.size(), 2, 10000);
    QCOMPARE(service->elevatedRequests.size(), 0);
    QVERIFY(service->perPackageRequests.contains(qMakePair(QStringLiteral("Alpha_pf"), true)));
    QVERIFY(service->perPackageRequests.contains(qMakePair(QStringLiteral("Beta_pf"), false)));

    QTRY_VERIFY_WITH_TIMEOUT(!applyButtonOf(*dialog)->isEnabled(), 10000);
}

void UwpLoopbackDialogTests::failedUnelevatedBatchKeepsTheRowsPending()
{
    auto service = std::make_shared<FakeUwpLoopbackService>();
    service->packages = {makePackage(QStringLiteral("Alpha_pf"), false)};
    service->elevatedResult = OperationResult::fail(QStringLiteral("elevated helper was denied"));
    const std::unique_ptr<UwpLoopbackDialog> dialog = makeDialog(service, false);
    dialog->show();
    QTRY_COMPARE_WITH_TIMEOUT(tableOf(*dialog)->rowCount(), 1, 10000);

    setRowChecked(*dialog, 0, true);
    applyButtonOf(*dialog)->click();

    QTRY_VERIFY_WITH_TIMEOUT(
        statusMessageOf(*dialog).contains(QStringLiteral("elevated helper was denied")), 10000);
    // A failure must not commit the pending change or drop it on the floor: the row stays dirty
    // so the user can retry.
    QVERIFY(applyButtonOf(*dialog)->isEnabled());
    QCOMPARE(tableOf(*dialog)->item(0, UwpLoopbackDialogSupport::EnabledColumn)->checkState(),
             Qt::Checked);
    QCOMPARE(service->listPackagesCalls, 1);
}

void UwpLoopbackDialogTests::failedPerPackageApplyNamesThePackage()
{
    auto service = std::make_shared<FakeUwpLoopbackService>();
    service->packages = {makePackage(QStringLiteral("Alpha_pf"), false),
                         makePackage(QStringLiteral("Beta_pf"), false)};
    service->failingPackages = QStringList{QStringLiteral("Alpha_pf"), QStringLiteral("Beta_pf")};
    const std::unique_ptr<UwpLoopbackDialog> dialog = makeDialog(service, true);
    dialog->show();
    QTRY_COMPARE_WITH_TIMEOUT(tableOf(*dialog)->rowCount(), 2, 10000);

    setRowChecked(*dialog, 0, true);
    setRowChecked(*dialog, 1, true);
    applyButtonOf(*dialog)->click();

    QTRY_VERIFY_WITH_TIMEOUT(
        statusMessageOf(*dialog).contains(QStringLiteral("Alpha_pf: access denied")), 10000);
    // Both rows failed, so both are named: with one message per package the user has to be able
    // to tell which package was rejected.
    QVERIFY(statusMessageOf(*dialog).contains(QStringLiteral("Beta_pf: access denied")));
    QVERIFY(applyButtonOf(*dialog)->isEnabled());
    QCOMPARE(tableOf(*dialog)->item(0, UwpLoopbackDialogSupport::EnabledColumn)->checkState(),
             Qt::Checked);
}

void UwpLoopbackDialogTests::partialPerPackageFailureOnlyKeepsTheFailedRowPending()
{
    auto service = std::make_shared<FakeUwpLoopbackService>();
    service->packages = {makePackage(QStringLiteral("Alpha_pf"), false),
                         makePackage(QStringLiteral("Beta_pf"), false)};
    service->failingPackages = QStringList{QStringLiteral("Beta_pf")};
    const std::unique_ptr<UwpLoopbackDialog> dialog = makeDialog(service, true);
    dialog->show();
    QTRY_COMPARE_WITH_TIMEOUT(tableOf(*dialog)->rowCount(), 2, 10000);

    setRowChecked(*dialog, 0, true);
    setRowChecked(*dialog, 1, true);
    applyButtonOf(*dialog)->click();

    QTRY_COMPARE_WITH_TIMEOUT(service->perPackageRequests.size(), 2, 10000);
    QTRY_VERIFY_WITH_TIMEOUT(
        statusMessageOf(*dialog).contains(QStringLiteral("Beta_pf: access denied")), 10000);
    QVERIFY(applyButtonOf(*dialog)->isEnabled());

    // Alpha was committed and Beta was not, so a retry has to carry Beta alone. Sending Alpha
    // again would be harmless for the exemption itself, but it would mean the dialog kept
    // treating a row as pending after it had already been written -- which is what the
    // per-package bookkeeping in the success branch is there to prevent.
    service->perPackageRequests.clear();
    applyButtonOf(*dialog)->click();
    QTRY_COMPARE_WITH_TIMEOUT(service->perPackageRequests.size(), 1, 10000);
    QCOMPARE(service->perPackageRequests.constFirst().first, QStringLiteral("Beta_pf"));
}

void UwpLoopbackDialogTests::successfulPerPackageApplyClearsThePendingRow()
{
    auto service = std::make_shared<FakeUwpLoopbackService>();
    service->packages = {makePackage(QStringLiteral("Alpha_pf"), false)};
    const std::unique_ptr<UwpLoopbackDialog> dialog = makeDialog(service, true);
    dialog->show();
    QTRY_COMPARE_WITH_TIMEOUT(tableOf(*dialog)->rowCount(), 1, 10000);

    setRowChecked(*dialog, 0, true);
    applyButtonOf(*dialog)->click();

    QTRY_COMPARE_WITH_TIMEOUT(service->perPackageRequests.size(), 1, 10000);
    QCOMPARE(service->perPackageRequests.constFirst().first, QStringLiteral("Alpha_pf"));
    QCOMPARE(service->perPackageRequests.constFirst().second, true);
    QTRY_VERIFY_WITH_TIMEOUT(!applyButtonOf(*dialog)->isEnabled(), 10000);
}

QTEST_MAIN(UwpLoopbackDialogTests)

#include "UwpLoopbackDialogTests.moc"
