#pragma once

#include <QList>
#include <QPair>
#include <QString>

#include "domain/models/VmessItem.h"

class ShareUrlBuilder {
public:
    static QString build(const VmessItem& item);

private:
    static QString buildVmess(const VmessItem& item);
    static QString buildShadowsocks(const VmessItem& item);
    static QString buildSocks(const VmessItem& item);
    static QString buildTrojan(const VmessItem& item);
    static QString buildVless(const VmessItem& item);
    static QString buildHysteria2(const VmessItem& item);
    static QString buildTuic(const VmessItem& item);
    static QString buildWireguard(const VmessItem& item);
    static QString buildAnytls(const VmessItem& item);
    static QString buildNaive(const VmessItem& item);
    static QList<QPair<QString, QString>> buildStandardTransportEntries(const VmessItem& item, const QString& defaultSecurity);
    static QString buildQuery(const QList<QPair<QString, QString>>& entries);
    static QString buildRemark(const QString& remarks);
    static QString joinList(const QStringList& values);
    static QString encodeBase64(const QString& value);
    static QString wrapIpv6(const QString& address);
};
