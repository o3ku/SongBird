#include <QtTest>

#include <QLineEdit>
#include <QPushButton>

#include "ui/dialogs/GlobalHotkeyDialog.h"

class GlobalHotkeyDialogTests : public QObject {
    Q_OBJECT

private slots:
    void dialogOpensWithCancelButtonFocused();
    void capturedShortcutStoresUpstreamCompatibleKeyCode();
    void capturedSystemShortcutStoresUpstreamCompatibleKeyCode_data();
    void capturedSystemShortcutStoresUpstreamCompatibleKeyCode();
    void capturedAltPrintScreenSystemKeyStoresUpstreamCompatibleKeyCode();
    void capturedBackspaceAndDeleteShortcuts_data();
    void capturedBackspaceAndDeleteShortcuts();
    void setHotkeysDisplaysUpstreamCompatibleFunctionKey();
    void resetClearsConfiguredHotkeys();
};

void GlobalHotkeyDialogTests::dialogOpensWithCancelButtonFocused()
{
    GlobalHotkeyDialog dialog;
    dialog.show();
    QCoreApplication::processEvents();

    auto* cancelButton = dialog.findChild<QPushButton*>(QStringLiteral("globalHotkeyCancelButton"));
    QVERIFY(cancelButton != nullptr);
    QTRY_VERIFY(cancelButton->hasFocus());
    QCOMPARE(dialog.focusWidget(), static_cast<QWidget*>(cancelButton));
}

void GlobalHotkeyDialogTests::capturedShortcutStoresUpstreamCompatibleKeyCode()
{
    GlobalHotkeyDialog dialog;
    dialog.show();
    QCoreApplication::processEvents();

    auto* showFormEdit = dialog.findChild<QLineEdit*>(QStringLiteral("globalHotkeyEdit_0"));
    QVERIFY(showFormEdit != nullptr);

    showFormEdit->setFocus();
    QVERIFY(showFormEdit->hasFocus());
    QTest::keyClick(showFormEdit, Qt::Key_V, Qt::ControlModifier | Qt::AltModifier);

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

void GlobalHotkeyDialogTests::capturedSystemShortcutStoresUpstreamCompatibleKeyCode_data()
{
    QTest::addColumn<int>("key");
    QTest::addColumn<int>("expectedKeyCode");

    QTest::newRow("pause") << static_cast<int>(Qt::Key_Pause) << 7;
    QTest::newRow("caps-lock") << static_cast<int>(Qt::Key_CapsLock) << 8;
    QTest::newRow("print-screen") << static_cast<int>(Qt::Key_Print) << 30;
    QTest::newRow("num-lock") << static_cast<int>(Qt::Key_NumLock) << 114;
    QTest::newRow("scroll-lock") << static_cast<int>(Qt::Key_ScrollLock) << 115;
}

void GlobalHotkeyDialogTests::capturedSystemShortcutStoresUpstreamCompatibleKeyCode()
{
    QFETCH(int, key);
    QFETCH(int, expectedKeyCode);

    GlobalHotkeyDialog dialog;
    dialog.show();
    QCoreApplication::processEvents();

    auto* showFormEdit = dialog.findChild<QLineEdit*>(QStringLiteral("globalHotkeyEdit_0"));
    QVERIFY(showFormEdit != nullptr);

    showFormEdit->setFocus();
    QVERIFY(showFormEdit->hasFocus());
    QTest::keyClick(showFormEdit, static_cast<Qt::Key>(key), Qt::ControlModifier);

    const QList<GlobalHotkeyItem> hotkeys = dialog.hotkeys();
    QCOMPARE(hotkeys.size(), 1);
    QCOMPARE(hotkeys.constFirst().action, GlobalHotkeyAction::ShowForm);
    QVERIFY(hotkeys.constFirst().control);
    QVERIFY(!hotkeys.constFirst().alt);
    QVERIFY(!hotkeys.constFirst().shift);
    QVERIFY(hotkeys.constFirst().keyCode.has_value());
    QCOMPARE(hotkeys.constFirst().keyCode.value(), expectedKeyCode);
}

void GlobalHotkeyDialogTests::capturedAltPrintScreenSystemKeyStoresUpstreamCompatibleKeyCode()
{
    GlobalHotkeyDialog dialog;
    dialog.show();
    QCoreApplication::processEvents();

    auto* showFormEdit = dialog.findChild<QLineEdit*>(QStringLiteral("globalHotkeyEdit_0"));
    QVERIFY(showFormEdit != nullptr);

    showFormEdit->setFocus();
    QVERIFY(showFormEdit->hasFocus());
    QTest::keyClick(showFormEdit, Qt::Key_SysReq, Qt::ControlModifier | Qt::AltModifier);

    const QList<GlobalHotkeyItem> hotkeys = dialog.hotkeys();
    QCOMPARE(hotkeys.size(), 1);
    QCOMPARE(hotkeys.constFirst().action, GlobalHotkeyAction::ShowForm);
    QVERIFY(hotkeys.constFirst().control);
    QVERIFY(hotkeys.constFirst().alt);
    QVERIFY(!hotkeys.constFirst().shift);
    QVERIFY(hotkeys.constFirst().keyCode.has_value());
    QCOMPARE(hotkeys.constFirst().keyCode.value(), 30);
}

void GlobalHotkeyDialogTests::capturedBackspaceAndDeleteShortcuts_data()
{
    QTest::addColumn<int>("key");
    QTest::addColumn<int>("expectedKeyCode");
    QTest::addColumn<QString>("expectedText");

    QTest::newRow("backspace") << static_cast<int>(Qt::Key_Backspace) << 2 << QStringLiteral("Ctrl+Backspace");
    QTest::newRow("delete") << static_cast<int>(Qt::Key_Delete) << 32 << QStringLiteral("Ctrl+Del");
}

void GlobalHotkeyDialogTests::capturedBackspaceAndDeleteShortcuts()
{
    QFETCH(int, key);
    QFETCH(int, expectedKeyCode);
    QFETCH(QString, expectedText);

    GlobalHotkeyDialog dialog;
    dialog.show();
    QCoreApplication::processEvents();

    auto* showFormEdit = dialog.findChild<QLineEdit*>(QStringLiteral("globalHotkeyEdit_0"));
    QVERIFY(showFormEdit != nullptr);

    showFormEdit->setFocus();
    QVERIFY(showFormEdit->hasFocus());
    QTest::keyClick(showFormEdit, static_cast<Qt::Key>(key), Qt::ControlModifier);

    const QList<GlobalHotkeyItem> hotkeys = dialog.hotkeys();
    QCOMPARE(hotkeys.size(), 1);
    QCOMPARE(hotkeys.constFirst().action, GlobalHotkeyAction::ShowForm);
    QVERIFY(hotkeys.constFirst().control);
    QVERIFY(!hotkeys.constFirst().alt);
    QVERIFY(!hotkeys.constFirst().shift);
    QVERIFY(hotkeys.constFirst().keyCode.has_value());
    QCOMPARE(hotkeys.constFirst().keyCode.value(), expectedKeyCode);
    QCOMPARE(showFormEdit->text(), expectedText);
}

void GlobalHotkeyDialogTests::setHotkeysDisplaysUpstreamCompatibleFunctionKey()
{
    QList<GlobalHotkeyItem> hotkeys;
    GlobalHotkeyItem item;
    item.action = GlobalHotkeyAction::SystemProxySet;
    item.control = true;
    item.shift = true;
    item.keyCode = 91;
    hotkeys.append(item);

    GlobalHotkeyDialog dialog;
    dialog.setHotkeys(hotkeys);

    auto* setProxyEdit = dialog.findChild<QLineEdit*>(QStringLiteral("globalHotkeyEdit_2"));
    QVERIFY(setProxyEdit != nullptr);
    QCOMPARE(setProxyEdit->text(), QStringLiteral("Ctrl+Shift+F2"));
}

void GlobalHotkeyDialogTests::resetClearsConfiguredHotkeys()
{
    QList<GlobalHotkeyItem> hotkeys;
    GlobalHotkeyItem item;
    item.action = GlobalHotkeyAction::ShowForm;
    item.control = true;
    item.alt = true;
    item.keyCode = 65;
    hotkeys.append(item);

    GlobalHotkeyDialog dialog;
    dialog.setHotkeys(hotkeys);

    auto* resetButton = dialog.findChild<QPushButton*>(QStringLiteral("globalHotkeyResetButton"));
    QVERIFY(resetButton != nullptr);

    resetButton->click();

    QVERIFY(dialog.hotkeys().isEmpty());
    auto* showFormEdit = dialog.findChild<QLineEdit*>(QStringLiteral("globalHotkeyEdit_0"));
    QVERIFY(showFormEdit != nullptr);
    QVERIFY(showFormEdit->text().isEmpty());
}

QTEST_MAIN(GlobalHotkeyDialogTests)

#include "GlobalHotkeyDialogTests.moc"
