#include "ui/mainwindow/MainWindowTitleSupport.h"

#include <QFontMetrics>
#include <QObject>

QString MainWindowTitleSupport::formatWindowTitle(
    const QString& coreName,
    const QString& serverName,
    ProxyUiState proxyUiState,
    bool tunEnabled,
    const QFontMetrics& fontMetrics,
    int serverNameMaximumWidth)
{
    const QString displayedCoreName = coreName.trimmed().isEmpty()
        ? QObject::tr("Unknown")
        : coreName.trimmed();
    const QString displayedServerName = serverName.trimmed().isEmpty()
        ? QObject::tr("None")
        : fontMetrics.elidedText(
            serverName.trimmed(),
            Qt::ElideRight,
            serverNameMaximumWidth);
    const QString proxyState = proxyUiState == ProxyUiState::Active
        ? QObject::tr("Proxy ON")
        : QObject::tr("Proxy OFF");
    const QString tunState = tunEnabled ? QObject::tr("TUN ON") : QObject::tr("TUN OFF");

    return QObject::tr("SongBird [%1] [%2] [%3] [%4]")
        .arg(displayedCoreName, displayedServerName, proxyState, tunState);
}
