#include "platform/SecretStoreFactory.h"

#include "platform/null/NullSecretStore.h"

std::unique_ptr<ISecretStore> createSecretStore()
{
    return std::make_unique<NullSecretStore>();
}
