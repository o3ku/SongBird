#include "runtime/core/CoreDescriptorRegistry.h"

namespace {

QList<CoreDescriptor>& mutableCoreDescriptors()
{
    static QList<CoreDescriptor> descriptors;
    return descriptors;
}

} // namespace

CoreDescriptorRegistration::CoreDescriptorRegistration(const CoreDescriptor& descriptor)
{
    QList<CoreDescriptor>& descriptors = mutableCoreDescriptors();
    for (const CoreDescriptor& existing : descriptors) {
        if (existing.type == descriptor.type) {
            return;
        }
    }
    descriptors.append(descriptor);
}

QList<CoreDescriptor> registeredCoreDescriptors()
{
    return mutableCoreDescriptors();
}
