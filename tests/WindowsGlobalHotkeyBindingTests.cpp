#include <QtTest>

#include <windows.h>

#include "platform/windows/WindowsGlobalHotkeyBinding.h"

class WindowsGlobalHotkeyBindingTests : public QObject {
    Q_OBJECT

private slots:
    void buildBindingsSkipsItemsWithoutKeyCodes();
    void buildBindingsMapsUpstreamKeyCodesAndModifiers();
    void buildBindingsMapsUpstreamSystemKeys_data();
    void buildBindingsMapsUpstreamSystemKeys();
    void buildBindingsMapsUpstreamOemKeys();
    void buildBindingsFallbackToLegacyQtKeyCodes();
    void buildBindingsReuseIdsForSharedShortcuts();
};

void WindowsGlobalHotkeyBindingTests::buildBindingsSkipsItemsWithoutKeyCodes()
{
    QList<GlobalHotkeyItem> hotkeys;

    GlobalHotkeyItem showForm;
    showForm.action = GlobalHotkeyAction::ShowForm;
    showForm.control = true;
    showForm.alt = true;
    showForm.keyCode = 65;
    hotkeys.append(showForm);

    GlobalHotkeyItem unset;
    unset.action = GlobalHotkeyAction::SystemProxySet;
    hotkeys.append(unset);

    const QList<WindowsGlobalHotkeyBinding> bindings = buildWindowsGlobalHotkeyBindings(hotkeys);

    QCOMPARE(bindings.size(), 1);
    QCOMPARE(bindings.constFirst().action, GlobalHotkeyAction::ShowForm);
    QCOMPARE(bindings.constFirst().virtualKey, static_cast<unsigned int>('V'));
}

void WindowsGlobalHotkeyBindingTests::buildBindingsMapsUpstreamKeyCodesAndModifiers()
{
    QList<GlobalHotkeyItem> hotkeys;

    GlobalHotkeyItem item;
    item.action = GlobalHotkeyAction::SystemProxySet;
    item.control = true;
    item.shift = true;
    item.keyCode = 91;
    hotkeys.append(item);

    const QList<WindowsGlobalHotkeyBinding> bindings = buildWindowsGlobalHotkeyBindings(hotkeys);

    QCOMPARE(bindings.size(), 1);
    QCOMPARE(bindings.constFirst().action, GlobalHotkeyAction::SystemProxySet);
    QCOMPARE(bindings.constFirst().modifiers & MOD_CONTROL, static_cast<unsigned int>(MOD_CONTROL));
    QCOMPARE(bindings.constFirst().modifiers & MOD_SHIFT, static_cast<unsigned int>(MOD_SHIFT));
    QCOMPARE(bindings.constFirst().virtualKey, static_cast<unsigned int>(VK_F2));
    QCOMPARE(bindings.constFirst().shortcut, QStringLiteral("Ctrl+Shift+F2"));
}

void WindowsGlobalHotkeyBindingTests::buildBindingsMapsUpstreamSystemKeys_data()
{
    QTest::addColumn<int>("keyCode");
    QTest::addColumn<unsigned int>("expectedVirtualKey");

    QTest::newRow("pause") << 7 << static_cast<unsigned int>(VK_PAUSE);
    QTest::newRow("caps-lock") << 8 << static_cast<unsigned int>(VK_CAPITAL);
    QTest::newRow("print-screen") << 30 << static_cast<unsigned int>(VK_SNAPSHOT);
    QTest::newRow("num-lock") << 114 << static_cast<unsigned int>(VK_NUMLOCK);
    QTest::newRow("scroll-lock") << 115 << static_cast<unsigned int>(VK_SCROLL);
}

void WindowsGlobalHotkeyBindingTests::buildBindingsMapsUpstreamSystemKeys()
{
    QFETCH(int, keyCode);
    QFETCH(unsigned int, expectedVirtualKey);

    QList<GlobalHotkeyItem> hotkeys;

    GlobalHotkeyItem item;
    item.action = GlobalHotkeyAction::ShowForm;
    item.control = true;
    item.keyCode = keyCode;
    hotkeys.append(item);

    const QList<WindowsGlobalHotkeyBinding> bindings = buildWindowsGlobalHotkeyBindings(hotkeys);

    QCOMPARE(bindings.size(), 1);
    QCOMPARE(bindings.constFirst().virtualKey, expectedVirtualKey);
}

void WindowsGlobalHotkeyBindingTests::buildBindingsMapsUpstreamOemKeys()
{
    QList<GlobalHotkeyItem> hotkeys;

    GlobalHotkeyItem item;
    item.action = GlobalHotkeyAction::ShowForm;
    item.control = true;
    item.keyCode = 140;
    hotkeys.append(item);

    const QList<WindowsGlobalHotkeyBinding> bindings = buildWindowsGlobalHotkeyBindings(hotkeys);

    QCOMPARE(bindings.size(), 1);
    QCOMPARE(bindings.constFirst().virtualKey, static_cast<unsigned int>(VK_OEM_1));
}

void WindowsGlobalHotkeyBindingTests::buildBindingsFallbackToLegacyQtKeyCodes()
{
    QList<GlobalHotkeyItem> hotkeys;

    GlobalHotkeyItem item;
    item.action = GlobalHotkeyAction::SystemProxySet;
    item.control = true;
    item.keyCode = static_cast<int>(Qt::Key_F2);
    hotkeys.append(item);

    const QList<WindowsGlobalHotkeyBinding> bindings = buildWindowsGlobalHotkeyBindings(hotkeys);

    QCOMPARE(bindings.size(), 1);
    QCOMPARE(bindings.constFirst().virtualKey, static_cast<unsigned int>(VK_F2));
    QCOMPARE(bindings.constFirst().shortcut, QStringLiteral("Ctrl+F2"));
}

void WindowsGlobalHotkeyBindingTests::buildBindingsReuseIdsForSharedShortcuts()
{
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

    const QList<WindowsGlobalHotkeyBinding> bindings = buildWindowsGlobalHotkeyBindings(hotkeys);

    QCOMPARE(bindings.size(), 2);
    QCOMPARE(bindings.at(0).id, bindings.at(1).id);
    QCOMPARE(bindings.at(0).shortcut, QStringLiteral("Ctrl+Alt+Shift+F24"));
    QCOMPARE(bindings.at(1).shortcut, QStringLiteral("Ctrl+Alt+Shift+F24"));
}

QTEST_MAIN(WindowsGlobalHotkeyBindingTests)

#include "WindowsGlobalHotkeyBindingTests.moc"
