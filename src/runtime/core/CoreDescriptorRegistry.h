#pragma once

#include <QList>

#include "runtime/core/CoreDescriptor.h"

class CoreDescriptorRegistration final {
public:
    explicit CoreDescriptorRegistration(const CoreDescriptor& descriptor);
};

QList<CoreDescriptor> registeredCoreDescriptors();
