#include "persistence/JsonConfigSerialization.h"

#include "persistence/JsonConfigCollectionSerialization.h"
#include "persistence/JsonConfigPolicySerialization.h"
#include "persistence/JsonConfigRootSerialization.h"
#include "persistence/JsonConfigTunSerialization.h"
#include "persistence/JsonConfigUiSerialization.h"

namespace JsonConfigSerialization {

Config parseConfig(const QJsonObject& root)
{
    Config config;
    JsonConfigRootSerialization::read(root, config.root());
    JsonConfigUiSerialization::read(root, config.ui());
    JsonConfigCollectionSerialization::read(root, config.collection());
    JsonConfigTunSerialization::read(root, config.tun());
    JsonConfigPolicySerialization::read(root, config.policy());
    return config;
}

void applyConfig(QJsonObject& root, const Config& config)
{
    root = QJsonObject();
    JsonConfigRootSerialization::write(root, config.root());
    JsonConfigUiSerialization::write(root, config.ui());
    JsonConfigCollectionSerialization::write(root, config.collection());
    JsonConfigTunSerialization::write(root, config.tun());
    JsonConfigPolicySerialization::write(root, config.policy());
}

} // namespace JsonConfigSerialization
