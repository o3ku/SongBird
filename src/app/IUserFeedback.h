#pragma once

#include <QString>

class QObject;
class QWidget;
struct OperationResult;

class IUserFeedback {
public:
    enum class TransientStatusPriority {
        Routine
    };

    enum class YesNoDefault {
        Yes,
        No
    };

    virtual ~IUserFeedback() = default;

    virtual QObject* uiContext() const = 0;
    virtual QWidget* dialogParent() const = 0;

    virtual void appendLog(const QString& message) = 0;
    virtual void showTransientStatus(
        const QString& message,
        int timeoutMs,
        TransientStatusPriority priority = TransientStatusPriority::Routine) = 0;
    virtual void recordOperationResult(const OperationResult& result) = 0;
    virtual void showOperationMessage(const QString& title, const OperationResult& result, QWidget* parent) = 0;
    virtual bool askYesNo(
        QWidget* parent,
        const QString& title,
        const QString& text,
        YesNoDefault defaultButton) = 0;
    virtual void showInformation(QWidget* parent, const QString& title, const QString& text) = 0;
    virtual void showTrayMessage(const QString& title, const QString& message, bool critical, int timeoutMs) = 0;
    virtual void openExternalUrl(const QString& url) = 0;
    virtual bool promptRestartForDownloadedAppUpdate(const QString& newExecutablePath, QWidget* parent) = 0;
};
