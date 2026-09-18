#pragma once

#include <QString>

#include "domain/models/RuntimeState.h"

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
