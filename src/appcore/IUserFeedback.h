#pragma once

#include <QString>

class QObject;
class QWidget;
struct OperationResult;

class IUserFeedback {
public:
    enum class YesNoDefault {
        Yes,
        No
    };

    virtual ~IUserFeedback() = default;

    virtual QObject* uiContext() const = 0;
    virtual QWidget* dialogParent() const = 0;

    virtual void appendLog(const QString& message) = 0;
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
    // A downloaded macOS disk image is not an executable the restart-and-apply path can swap in,
    // so the host offers to open it instead: Finder mounts it and the user drags SongBird to
    // Applications. Returns true when the package was opened.
    virtual bool promptOpenDownloadedAppUpdatePackage(const QString& packagePath, QWidget* parent) = 0;
};
