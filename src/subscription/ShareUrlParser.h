#pragma once

#include <QList>
#include <QString>

#include "domain/models/VmessItem.h"

class ShareUrlParser {
public:
    static VmessItem parse(const QString& shareUrl, bool* ok = nullptr);
    static QList<VmessItem> parseMany(const QString& text);
};
