#pragma once

#include "domain/models/VmessItem.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace AddServerDialogOptions {

struct ComboOption {
    QString label;
    int value = 0;
};

struct ProtocolFieldState {
    QString credentialLabelText;
    QString securityLabelText;
    QString defaultSecurity;
    bool showAlterId = false;
    bool showFlow = false;
};

struct TransportFieldState {
    QString headerLabelText;
    QString hostLabelText;
    QString pathLabelText;
    QString hostPlaceholder;
    QString pathPlaceholder;
    bool showHost = true;
    bool secureTransport = false;
    bool tlsTransport = false;
    bool realityTransport = false;
    bool httpupgradeTransport = false;
    bool xhttpTransport = false;
};

QList<ComboOption> protocolTypeOptions();
ProtocolFieldState protocolFieldState(ConfigType type);
TransportFieldState transportFieldState(const QString& network, const QString& transportSecurity);

QStringList securityOptions(ConfigType type);
QStringList networkOptions();
QStringList headerTypeOptions();
QStringList streamSecurityOptions();
QStringList echForceQueryOptions();
QStringList userAgentOptions();
QStringList allowInsecureOptions();
QStringList congestionControlOptions();
QStringList udpRelayModeOptions();

} // namespace AddServerDialogOptions
