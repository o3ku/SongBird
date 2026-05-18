#pragma once

#include <functional>

#include <QList>

#include "domain/models/RoutingItem.h"

class QComboBox;

class RoutingModeController final {
public:
    explicit RoutingModeController(
        QComboBox* routingModeCombo,
        std::function<void(int)> routingModeSelected);

    void setup();
    void setRoutingOptions(const QList<RoutingItem>& routingItems, int routingIndex);

private:
    static QString describeRoutingMode(const RoutingItem& item, int index);

    QComboBox* routingModeCombo_ = nullptr;
    std::function<void(int)> routingModeSelected_;
};
