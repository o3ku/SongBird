#pragma once

#include <QString>

#include "app/RuntimeState.h"

class QFontMetrics;

namespace MainWindowTitleSupport {

QString formatWindowTitle(
    const QString& coreName,
    const QString& serverName,
    ProxyUiState proxyUiState,
    bool tunEnabled,
    const QFontMetrics& fontMetrics,
    int serverNameMaximumWidth);

} // namespace MainWindowTitleSupport
