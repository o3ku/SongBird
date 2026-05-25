#pragma once

#include <QString>

#include "domain/models/VmessItem.h"

class ShareUrlBuilder {
public:
    static QString build(const VmessItem& item);
};
