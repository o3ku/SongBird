#include "platform/SecretStoreFactory.h"

#include "platform/macos/MacSecretStore.h"

std::unique_ptr<ISecretStore> createSecretStore()
{
    return std::make_unique<MacSecretStore>();
}
