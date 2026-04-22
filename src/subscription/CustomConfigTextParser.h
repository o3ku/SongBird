#pragma once

#include <QString>

#include "domain/models/VmessItem.h"

class CustomConfigTextParser {
public:
    static VmessItem parse(const QString& text, QString* fileExtension = nullptr, bool* ok = nullptr);
};
