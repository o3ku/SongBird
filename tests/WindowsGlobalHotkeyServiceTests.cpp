#include <QtTest>

#include <QLineEdit>
#include <QSignalSpy>

#define private public
#include "platform/windows/WindowsGlobalHotkeyService.h"
#undef private
#include "ui/dialogs/GlobalHotkeyDialog.h"

#include <windows.h>

class WindowsGlobalHotkeyServiceTests : public QObject {
    Q_OBJECT

private slots:
    void pausedServiceSuppressesHotkeyDispatch();
    void pausedServiceReplaysHotkeyIntoFocusedEditor();
    void registerHotkeysAggregatesSharedShortcutActions();
};

void WindowsGlobalHotkeyServiceTests::pausedServiceSuppressesHotkeyDispatch()
{
    WindowsGlobalHotkeyService service;
    service.registeredActions_[1].append(GlobalHotkeyAction::ShowForm);

    QSignalSpy spy(&service, SIGNAL(toggleMainWindowRequested()));
    QVERIFY(spy.isValid());

    MSG nativeMessage{};
    nativeMessage.message = WM_HOTKEY;
    nativeMessage.wParam = 1;
    WindowsGlobalHotkeyService::NativeEventResult result = 0;

    service.setPaused(true);
    QVERIFY(service.nativeEventFilter(QByteArrayLiteral("windows_generic_MSG"), &nativeMessage, &result));
    QCOMPARE(spy.count(), 0);

    service.setPaused(false);
    QVERIFY(service.nativeEventFilter(QByteArrayLiteral("windows_generic_MSG"), &nativeMessage, &result));
    QCOMPARE(spy.count(), 1);
}

void WindowsGlobalHotkeyServiceTests::pausedServiceReplaysHotkeyIntoFocusedEditor()
{
    WindowsGlobalHotkeyService service;
    service.registeredActions_[1].append(GlobalHotkeyAction::ShowForm);

    QSignalSpy spy(&service, SIGNAL(toggleMainWindowRequested()));
    QVERIFY(spy.isValid());

    GlobalHotkeyDialog dialog;
    dialog.show();
    QCoreApplication::processEvents();

    auto* showFormEdit = dialog.findChild<QLineEdit*>(QStringLiteral("globalHotkeyEdit_0"));
    QVERIFY(showFormEdit != nullptr);

    showFormEdit->setFocus();
    QVERIFY(showFormEdit->hasFocus());

    MSG nativeMessage{};
    nativeMessage.message = WM_HOTKEY;
    nativeMessage.wParam = 1;
    nativeMessage.lParam = MAKELPARAM(MOD_CONTROL | MOD_ALT, 'V');
    WindowsGlobalHotkeyService::NativeEventResult result = 0;

    service.setPaused(true);
    QVERIFY(service.nativeEventFilter(QByteArrayLiteral("windows_generic_MSG"), &nativeMessage, &result));
    QCOMPARE(spy.count(), 0);

    const QList<GlobalHotkeyItem> hotkeys = dialog.hotkeys();
    QCOMPARE(hotkeys.size(), 1);
    QCOMPARE(hotkeys.constFirst().action, GlobalHotkeyAction::ShowForm);
    QVERIFY(hotkeys.constFirst().control);
    QVERIFY(hotkeys.constFirst().alt);
    QVERIFY(!hotkeys.constFirst().shift);
    QVERIFY(hotkeys.constFirst().keyCode.has_value());
    QCOMPARE(hotkeys.constFirst().keyCode.value(), 65);
    QCOMPARE(showFormEdit->text(), QStringLiteral("Ctrl+Alt+V"));
}

void WindowsGlobalHotkeyServiceTests::registerHotkeysAggregatesSharedShortcutActions()
{
    WindowsGlobalHotkeyService service;

    QList<GlobalHotkeyItem> hotkeys;
    GlobalHotkeyItem showForm;
    showForm.action = GlobalHotkeyAction::ShowForm;
    showForm.control = true;
    showForm.alt = true;
    showForm.shift = true;
    showForm.keyCode = 113;
    hotkeys.append(showForm);

    GlobalHotkeyItem setProxy = showForm;
    setProxy.action = GlobalHotkeyAction::SystemProxySet;
    hotkeys.append(setProxy);

    const OperationResult result = service.registerHotkeys(hotkeys);
    QVERIFY2(result.success, qPrintable(result.message));
    QCOMPARE(service.registeredActions_.size(), 1);
    QCOMPARE(service.registeredIds_.size(), 1);

    const QList<GlobalHotkeyAction> actions = service.registeredActions_.constBegin().value();
    QCOMPARE(actions.size(), 2);
    QVERIFY(actions.contains(GlobalHotkeyAction::ShowForm));
    QVERIFY(actions.contains(GlobalHotkeyAction::SystemProxySet));
}

QTEST_MAIN(WindowsGlobalHotkeyServiceTests)

#include "WindowsGlobalHotkeyServiceTests.moc"
