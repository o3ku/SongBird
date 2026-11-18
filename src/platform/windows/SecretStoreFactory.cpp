#include "platform/SecretStoreFactory.h"

#include "platform/windows/WindowsSecretStore.h"

std::unique_ptr<ISecretStore> createSecretStore()
{
    return std::make_unique<WindowsSecretStore>();
}
