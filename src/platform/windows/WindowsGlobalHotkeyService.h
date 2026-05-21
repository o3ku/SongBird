#pragma once

#include <QAbstractNativeEventFilter>
#include <QHash>
#include <QObject>
#include <QString>

#include "common/OperationResult.h"
#include "domain/models/GlobalHotkeyItem.h"

class WindowsGlobalHotkeyService final : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT

public:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    using NativeEventResult = qintptr;
#else
    using NativeEventResult = long;
#endif

    explicit WindowsGlobalHotkeyService(QObject* parent = nullptr);
    ~WindowsGlobalHotkeyService() override;

    OperationResult registerHotkeys(const QList<GlobalHotkeyItem>& hotkeys);
    void unregisterHotkeys();
    void setPaused(bool paused);

    bool nativeEventFilter(const QByteArray& eventType, void* message, NativeEventResult* result) override;

signals:
    void toggleMainWindowRequested();
    void enableProxyRequested();
    void disableProxyRequested();

private:
    QString buildRegistrationMessage(const QList<QString>& successMessages, const QList<QString>& failureMessages) const;

    bool filterInstalled_ = false;
    bool paused_ = false;
    QList<int> registeredIds_;
    QHash<int, QList<GlobalHotkeyAction>> registeredActions_;
};
